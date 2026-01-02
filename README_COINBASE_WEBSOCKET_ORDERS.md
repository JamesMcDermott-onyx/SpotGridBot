# Coinbase WebSocket Order Connection (ConnectionORDWS)

## Overview

This implementation provides WebSocket-based order management for Coinbase Advanced Trade API, as an alternative to the REST-based `ConnectionORD`. The WebSocket implementation offers real-time order updates and lower latency for order operations.

## Features

- **WebSocket-based order management**: Place, cancel, and monitor orders via WebSocket
- **JWT authentication**: Uses JWT tokens for secure WebSocket authentication
- **Real-time order updates**: Receive instant notifications on order status changes
- **Protocol switching**: Easily switch between REST and WebSocket via configuration

## Architecture

### Key Components

1. **ConnectionORDWS.h/cpp**: Main WebSocket order connection class
   - Inherits from `ConnectionBase` (WebSocket foundation)
   - Implements order management methods (SendOrder, CancelOrder, GetOrders)
   - Handles real-time order updates from the exchange

2. **Message Handlers**:
   - `OnOrderUpdate()`: Processes order status updates
   - `OnOrderResponse()`: Handles order placement/cancellation responses
   - `OnOrderError()`: Manages error messages

3. **JWT Signing**: Uses `JWTGenerator.h` for authentication

## Configuration

### Basic Configuration

To use WebSocket-based orders, set the `protocol` attribute to "ws" in your `config.xml`:

```xml
<Session name="COINBASEORD"
         num_id="4"
         protocol="ws"
         host="advanced-trade-ws.coinbase.com"
         port="443"
         api_key="organizations/.../apiKeys/..."
         secret_key="-----BEGIN EC PRIVATE KEY-----\n...\n-----END EC PRIVATE KEY-----\n"
         passphrase="your_passphrase"
         schema="Coinbase:ORD"
/>
```

### REST Configuration (Default)

For REST-based orders, set `protocol="rest"`:

```xml
<Session name="COINBASEORD"
         num_id="4"
         protocol="rest"
         orders_http="https://api.coinbase.com/api/v3/brokerage/"
         api_key="organizations/.../apiKeys/..."
         secret_key="-----BEGIN EC PRIVATE KEY-----\n...\n-----END EC PRIVATE KEY-----\n"
         passphrase="your_passphrase"
         schema="Coinbase:ORD"
/>
```

## Protocol Switching

The system automatically selects the appropriate connection class based on the `protocol` attribute:

- `protocol="rest"` → Uses `ConnectionORD` (REST API)
- `protocol="ws"` → Uses `ConnectionORDWS` (WebSocket)

The schema is dynamically adjusted in `ConnectionManager::LoadConfig()`:
- REST: `Coinbase:ORD`
- WebSocket: `Coinbase:ORDWS`

## Implementation Details

### Order Placement

```cpp
std::string SendOrder(
    const UTILS::CurrencyPair &instrument,
    const UTILS::Side side,
    const RESTAPI::EOrderType orderType,
    const UTILS::TimeInForce timeInForce,
    const double price,
    const double quantity,
    const std::string &clientOrderId = ""
);
```

Example WebSocket message sent:
```json
{
  "type": "order",
  "action": "create",
  "client_order_id": "ws_1234567890_0",
  "product_id": "BTC-USD",
  "side": "BUY",
  "order_configuration": {
    "limit_limit_gtc": {
      "base_size": "0.001",
      "limit_price": "30000.0",
      "post_only": false
    }
  },
  "jwt": "eyJ..."
}
```

### Order Cancellation

```cpp
std::string CancelOrder(
    const UTILS::CurrencyPair &instrument,
    const std::string &orderId,
    const std::optional<std::string> &origClientOrderId = std::nullopt
);
```

### Subscription

The connection automatically subscribes to the "user" channel to receive order updates:

```json
{
  "type": "subscribe",
  "channel": "user",
  "product_ids": ["BTC-USD"],
  "jwt": "eyJ..."
}
```

## Message Flow

1. **Order Placement**:
   - Client → `SendOrder()` → JWT-signed WebSocket message → Coinbase
   - Coinbase → Order response → `OnOrderResponse()`
   - Coinbase → Order update (NEW) → `OnOrderUpdate()`

2. **Order Execution**:
   - Coinbase → Order update (FILLED/PARTIALLY_FILLED) → `OnOrderUpdate()`
   - System translates to `ExecutionReportData`

3. **Order Cancellation**:
   - Client → `CancelOrder()` → JWT-signed WebSocket message → Coinbase
   - Coinbase → Order update (CANCELLED) → `OnOrderUpdate()`

## Differences from REST Implementation

| Feature | REST (ConnectionORD) | WebSocket (ConnectionORDWS) |
|---------|---------------------|----------------------------|
| Base Class | `RestConnectionBase` | `ConnectionBase` |
| Authentication | HMAC signature per request | JWT token per message |
| Order Updates | Polling required | Push-based real-time |
| Connection | Stateless | Stateful (persistent) |
| Latency | Higher | Lower |
| Rate Limiting | Per-request limits | Connection-based |

## Advantages of WebSocket

1. **Lower Latency**: Real-time updates without polling
2. **Reduced Load**: Single connection vs. multiple HTTP requests
3. **Better UX**: Instant order status updates
4. **Efficiency**: No need for periodic order status queries

## Limitations

1. **Connection Management**: Requires maintaining persistent connection
2. **Reconnection Logic**: Need to handle disconnections and resubscriptions
3. **Complexity**: More complex than stateless REST
4. **Testing**: Harder to debug than REST calls

## Future Enhancements

1. **Order Book Integration**: Combine market data and orders on same WebSocket
2. **Batch Operations**: Support multiple orders in single message
3. **Advanced Order Types**: Stop-loss, trailing stop, etc.
4. **Position Management**: Real-time position tracking
5. **Error Recovery**: Automatic reconnection and state recovery
6. **Order Cache**: Maintain local order state for faster queries

## Testing

To test the WebSocket order connection:

1. Update `config.xml` with `protocol="ws"`
2. Ensure valid API credentials with order permissions
3. Run the application and monitor logs
4. Place a test order and verify WebSocket messages in logs

## References

- [Coinbase Advanced Trade WebSocket API](https://docs.cloud.coinbase.com/advanced-trade-api/docs/ws-overview)
- [Coinbase WebSocket Channels](https://docs.cloud.coinbase.com/advanced-trade-api/docs/ws-channels)
- [JWT Authentication](https://docs.cloud.coinbase.com/advanced-trade-api/docs/ws-auth)

## Files Modified/Created

- `include/coinbase/ConnectionORDWS.h` - Header file
- `src/coinbase/ConnectionORDWS.cpp` - Implementation
- `include/SchemaDefs.h` - Added `SCHEMAORDWS`
- `src/ConnectionManager.cpp` - Added WebSocket connection registration and protocol switching logic
