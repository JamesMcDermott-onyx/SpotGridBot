# GridStrategy WebSocket Order Update Migration

## Overview

The GridStrategy has been updated to work with the new WebSocket-based order connector that provides **push-based** order updates instead of requiring **polling** via REST API queries.

## Architecture Changes

### Before (REST-based Polling)
```
GridStrategy.CheckFilledOrders()
    ↓
OrderManager.GetOrder(cp, orderId)  ← Queries exchange via REST
    ↓
ConnectionORD.QueryOrder()  ← REST API call
    ↓
Exchange REST API
```

**Problems:**
- High latency - must wait for REST API round-trip
- API rate limits - frequent polling can hit rate limits
- Inefficient - queries even when nothing has changed
- Stale data - only updated when polled

### After (WebSocket Push Updates)
```
Exchange WebSocket (user channel)
    ↓
ConnectionORDWS.OnOrderUpdate()  ← Receives push notification
    ↓
OrderManager.UpdateOrder(orderId, status, filled)  ← Updates cache
    ↓
GridStrategy.CheckFilledOrders()
    ↓
OrderManager.GetOrderLocal(orderId)  ← Reads cached state (no network call)
```

**Benefits:**
- Real-time updates - instant notification of order changes
- No API rate limit issues - single WebSocket connection
- Efficient - only updates when something actually changes
- Always current - cache updated immediately on each change

## Code Changes

### 1. New Methods in IOrderManager Interface

```cpp
// WebSocket-based: Get locally cached order status 
// (use for WebSocket connections with push updates)
virtual std::optional<Order> GetOrderLocal(const std::string &orderId) = 0;

// Update order status from WebSocket push notifications
virtual void UpdateOrder(const std::string &orderId, OrderStatus status, double filled) = 0;

// Sync order from external source (e.g., startup snapshot) 
// Creates or updates order in cache without triggering placement
virtual void SyncOrder(const std::string &orderId, UTILS::Side side, double price, 
                       double quantity, OrderStatus status, double filled) = 0;
```

### 2. OrderManager Implementation

**GetOrderLocal()** - Returns cached order state without querying exchange:
```cpp
std::optional<Order> OrderManager::GetOrderLocal(const std::string &orderId)
{
    std::lock_guard<std::mutex> g(m_mutex);
    if (!m_orders.count(orderId))
        return {};
    return m_orders[orderId];
}
```

**UpdateOrder()** - Called by WebSocket connection when order status changes:
```cpp
void OrderManager::UpdateOrder(const std::string &orderId, OrderStatus status, double filled)
{
    std::lock_guard<std::mutex> g(m_mutex);
    if (!m_orders.count(orderId))
    {
        poco_warning_f1(logger(), "UpdateOrder called for unknown order: %s", orderId);
        return;
    }
    m_orders[orderId].status = status;
    m_orders[orderId].filled = filled;
    poco_information_f3(logger(), "Order updated: %s, status=%d, filled=%f", 
        orderId, static_cast<int>(status), filled);
}
```

**SyncOrder()** - Populates cache with existing orders during startup:
```cpp
void OrderManager::SyncOrder(const std::string &orderId, UTILS::Side side, double price, 
                             double quantity, OrderStatus status, double filled)
{
    std::lock_guard<std::mutex> g(m_mutex);
    
    if (m_orders.count(orderId))
    {
        // Update existing order
        m_orders[orderId].status = status;
        m_orders[orderId].filled = filled;
    }
    else
    {
        // Add new order to cache
        Order order;
        order.id = orderId;
        order.side = side;
        order.price = price;
        order.quantity = quantity;
        order.status = status;
        order.filled = filled;
        m_orders[orderId] = order;
    }
}
```

### 3. GridStrategy Changes

**Before:**
```cpp
// Query the exchange/order manager for the latest status of this order
auto maybe = m_orderManager->GetOrder(m_cp, orderId);
```

**After:**
```cpp
// Get the locally cached order status (updated by WebSocket push notifications)
// Note: With WebSocket connections, order status is pushed to OrderManager automatically,
// so we don't need to query the exchange - just read the cached state
auto maybe = m_orderManager->GetOrderLocal(orderId);
```

### 4. ConnectionORDWS Integration

The WebSocket connection processes two types of order events:

**Snapshot (Startup Sync)** - Populates cache with existing orders:
```cpp
// In "user" channel handler for event_type == "snapshot"
if (eventObj->has("orders"))
{
    auto orders = eventObj->getArray("orders");
    auto orderManager = m_connectionManager.GetOrderManager();
    
    for (each order in snapshot)
    {
        // Parse order details
        std::string order_id = orderObj->optValue<std::string>("order_id", "");
        std::string side_str = orderObj->optValue<std::string>("side", "");
        std::string status = orderObj->optValue<std::string>("status", "");
        // ... parse price, quantity, filled
        
        // Sync to OrderManager cache
        orderManager->SyncOrder(order_id, side, price, quantity, orderStatus, filled);
    }
    
    poco_information_f1(logger(), "Successfully synced %d orders to OrderManager cache", syncCount);
}
```

**Updates (Real-time)** - Pushes status changes to OrderManager:
```cpp
void ConnectionORDWS::OnOrderUpdate(const std::shared_ptr<CRYPTO::JSONDocument> jd)
{
    // Parse order update from WebSocket event
    // ...
    
    // Get OrderManager from ConnectionManager
    auto orderManager = m_connectionManager.GetOrderManager();
    if (orderManager)
    {
        // Translate Coinbase status to internal OrderStatus
        CORE::OrderStatus orderStatus;
        if (status == "FILLED" || status == "DONE")
            orderStatus = CORE::OrderStatus::FILLED;
        // ... other statuses
        
        // Push update to OrderManager
        orderManager->UpdateOrder(order_id, orderStatus, filled);
    }
}
```

### 5. ConnectionManager Wiring

ConnectionManager now holds a reference to OrderManager so WebSocket connections can push updates:

```cpp
// In main.cpp:
auto m_connectionManager = make_shared<ConnectionManager>(...);
auto m_orderManager = make_shared<OrderManager>(m_connectionManager);

// Set OrderManager reference so WebSocket connections can push updates
m_connectionManager->SetOrderManager(m_orderManager);
```

## Key Differences

| Aspect | REST (GetOrder) | WebSocket (GetOrderLocal) |
|--------|----------------|---------------------------|
| **Network Call** | Yes - queries exchange | No - reads cache |
| **Latency** | High (REST round-trip) | Low (memory access) |
| **Update Trigger** | Poll-based | Push-based (event-driven) |
| **Rate Limits** | Can hit limits | Single connection |
| **Data Freshness** | Stale until polled | Real-time |
| **API Efficiency** | One call per check | One subscription |

## Order Status Flow

### Startup - Order Sync
1. WebSocket connects to exchange
2. Subscribe to "user" channel → Exchange sends snapshot event
3. ConnectionORDWS receives snapshot with all active orders
4. For each order in snapshot:
   - Parse order details (id, side, price, quantity, status, filled)
   - Call `OrderManager::SyncOrder()` to populate cache
5. OrderManager logs: `"Successfully synced N orders to OrderManager cache"`
6. GridStrategy can now query these orders via `GetOrderLocal()`

**Benefits:**
- Bot can resume after restart and know about existing orders
- No lost orders when reconnecting
- Cache is immediately populated with current state

### Order Creation
1. GridStrategy calls `OrderManager::PlaceLimitOrder()`
2. OrderManager sends order via WebSocket
3. OrderManager stores order in `m_orders` map with status `NEW`
4. Exchange confirms order → WebSocket push update
5. ConnectionORDWS receives update → calls `OrderManager::UpdateOrder()`

### Order Fill (Partial or Full)
1. Exchange matches order → sends WebSocket update
2. ConnectionORDWS receives `"status": "FILLED"` or `"PARTIALLY_FILLED"`
3. ConnectionORDWS calls `OrderManager::UpdateOrder(orderId, FILLED, filledQty)`
4. OrderManager updates cache
5. GridStrategy calls `CheckFilledOrders()` (triggered by order book update)
6. GridStrategy reads updated status via `GetOrderLocal()` - **NO network call**
7. GridStrategy places hedge order

### Order Cancellation
1. GridStrategy calls `OrderManager::CancelOrder()`
2. OrderManager sends cancel via WebSocket
3. Exchange confirms cancellation → WebSocket push update
4. ConnectionORDWS updates OrderManager cache

## Migration Guide

If you're extending this code to other exchanges or strategies:

### For REST-based Connectors
Continue using `GetOrder()`:
```cpp
auto order = m_orderManager->GetOrder(cp, orderId);
```

### For WebSocket-based Connectors
Use `GetOrderLocal()`:
```cpp
auto order = m_orderManager->GetOrderLocal(orderId);
```

### Implementing New WebSocket Connectors
1. Subscribe to user/order update channel on connection
2. In your `OnOrderUpdate()` handler:
   ```cpp
   auto orderManager = m_connectionManager.GetOrderManager();
   if (orderManager) {
       orderManager->UpdateOrder(orderId, translatedStatus, filledQty);
   }
   ```
3. Translate exchange-specific status strings to `CORE::OrderStatus` enum

## Performance Impact

### Before (REST Polling)
- CheckFilledOrders() with 10 active orders = 10 REST API calls
- Each call: ~50-200ms latency
- Total: 500-2000ms to check all orders

### After (WebSocket Push)
- CheckFilledOrders() with 10 active orders = 0 network calls
- Each check: <1ms (memory access)
- Total: <10ms to check all orders
- Updates arrive proactively in <100ms after exchange processes fill

## Backward Compatibility

The `GetOrder()` method still exists and works for:
- REST-based connections that don't support WebSocket
- Manual refresh/verification scenarios
- Debugging and reconciliation

However, for WebSocket-based systems, `GetOrderLocal()` should be preferred for performance and efficiency.

## Testing

To verify the new system works:

1. Enable WebSocket order connection in config.xml:
   ```xml
   <Session name="COINBASEORD" protocol="ws" ... />
   ```

2. **Test Startup Sync:**
   - Place some orders manually on the exchange
   - Start the bot
   - Check logs for:
     ```
     Received user snapshot
     Snapshot contains N active orders - syncing to OrderManager
     Order synced (new): order_id_1
     Order synced (new): order_id_2
     Successfully synced N orders to OrderManager cache
     ```

3. **Test Real-time Updates:**
   - Place orders through the bot
   - Check logs for order creation and updates:
     ```
     Order update: id=xxx, client_id=xxx, status=FILLED
     Order updated: xxx, status=2, filled=0.001
     ```

4. **Test Bot Restart:**
   - Start bot with existing open orders
   - Verify GridStrategy sees the synced orders
   - Verify it doesn't try to re-place them

5. Verify GridStrategy responds to fills without seeing QueryOrder() calls in logs

## Troubleshooting

### Orders not updating
- Check WebSocket connection is established
- Verify user channel subscription succeeded
- Check ConnectionManager has OrderManager reference set
- Look for errors in `OnOrderUpdate()` processing

### Stale order data
- Ensure `SetOrderManager()` was called in main.cpp
- Verify WebSocket is receiving order updates (check logs)
- Confirm status translation is working correctly

### Missing orders in cache
- Orders must be placed through `OrderManager::PlaceLimitOrder()` to be tracked
- External orders (placed outside the system) won't be in cache unless explicitly added

## Future Enhancements

Potential improvements to this architecture:

1. **Periodic Reconciliation** - Occasionally use `GetOrder()` to verify cache accuracy against exchange
2. **Order Event Callbacks** - Allow strategies to register callbacks for specific order events
3. **Multi-Exchange Support** - Abstract the update mechanism for other exchanges (Binance, OKX)
4. **Order History Tracking** - Maintain history of filled/cancelled orders for analytics
