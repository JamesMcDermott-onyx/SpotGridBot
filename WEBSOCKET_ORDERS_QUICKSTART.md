# Coinbase WebSocket Order Connection - Quick Start

## Summary

I've created a new WebSocket-based order management connector for Coinbase (`ConnectionORDWS`) that complements the existing REST-based connector (`ConnectionORD`). You can now switch between protocols using the `protocol` attribute in your config.xml.

## Files Created/Modified

### New Files
1. **include/coinbase/ConnectionORDWS.h** - WebSocket order connection header
2. **src/coinbase/ConnectionORDWS.cpp** - WebSocket order connection implementation
3. **README_COINBASE_WEBSOCKET_ORDERS.md** - Detailed documentation

### Modified Files
1. **include/SchemaDefs.h** - Added `SCHEMAORDWS` constant
2. **src/ConnectionManager.cpp** - Added WebSocket connection registration and protocol-based schema selection

## Configuration

### Using WebSocket (New!)

```xml
<Session name="COINBASEORD"
         num_id="4"
         protocol="ws"
         host="advanced-trade-ws.coinbase.com"
         port="443"
         api_key="organizations/.../apiKeys/..."
         secret_key="-----BEGIN EC PRIVATE KEY-----\n...\n-----END EC PRIVATE KEY-----\n"
         passphrase="thsv5x4pwhmo"
         schema="Coinbase:ORD"
/>
```

### Using REST (Existing)

```xml
<Session name="COINBASEORD"
         num_id="4"
         protocol="rest"
         orders_http="https://api-sandbox.coinbase.com/api/v3/brokerage/"
         api_key="organizations/.../apiKeys/..."
         secret_key="-----BEGIN EC PRIVATE KEY-----\n...\n-----END EC PRIVATE KEY-----\n"
         port="443"
         passphrase="thsv5x4pwhmo"
         schema="Coinbase:ORD"
/>
```

## How It Works

The `ConnectionManager` now automatically selects the appropriate connector based on the `protocol` attribute:

- `protocol="rest"` → Uses `ConnectionORD` (REST API, schema: `Coinbase:ORD`)
- `protocol="ws"` → Uses `ConnectionORDWS` (WebSocket, schema: `Coinbase:ORDWS`)

This is handled transparently in `ConnectionManager::LoadConfig()`.

## Key Features

### WebSocket Advantages
- ✅ **Real-time order updates** - Instant push notifications
- ✅ **Lower latency** - No polling required
- ✅ **Single connection** - More efficient than multiple REST calls
- ✅ **JWT authentication** - Secure token-based auth

### API Coverage
- ✅ **SendOrder()** - Place market/limit orders
- ✅ **CancelOrder()** - Cancel existing orders
- ✅ **GetOrders()** - Request order list
- ✅ **Real-time updates** - Automatic order status notifications

### Order Status Translation
Maps Coinbase statuses to internal FIX protocol statuses:
- `OPEN/PENDING` → NEW
- `FILLED/DONE` → FILLED
- `CANCELLED` → CANCELED
- `REJECTED/FAILED` → REJECTED
- `PARTIALLY_FILLED` → PARTIALLY_FILLED
- `EXPIRED` → EXPIRED

## Testing

To test the new connector:

1. Update your `config.xml` to use `protocol="ws"`
2. Build and run: `make && ./src/SpotGridBot`
3. Check logs for WebSocket connection and order messages
4. Place a test order and verify real-time updates

## Implementation Details

### Architecture
```
ConnectionBase (WebSocket)
    ↑
    |
ConnectionORDWS
    - Manages WebSocket connection
    - Signs messages with JWT
    - Handles order lifecycle
    - Translates execution reports
```

### Message Flow
```
Your Code → SendOrder()
    ↓
ConnectionORDWS → JWT-signed WebSocket message
    ↓
Coinbase Advanced Trade API
    ↓
Order Response → OnOrderResponse()
    ↓
Order Updates → OnOrderUpdate()
    ↓
ExecutionReportData → Your System
```

## Next Steps

1. **Test with sandbox** - Use `https://api-sandbox.coinbase.com` for testing
2. **Monitor logs** - Check order lifecycle in application logs
3. **Error handling** - Implement reconnection logic if needed
4. **Rate limits** - Monitor WebSocket connection stability

## Additional Documentation

See `README_COINBASE_WEBSOCKET_ORDERS.md` for:
- Detailed architecture
- Complete API reference
- Comparison with REST
- Future enhancements
- Troubleshooting guide

## Example Usage in Code

The order management interface remains the same whether using REST or WebSocket:

```cpp
// Same code works with both REST and WebSocket!
auto orderId = orderManager->SendOrder(
    currencyPair,
    Side::BUY,
    EOrderType::Limit,
    TimeInForce::GTC,
    price,
    quantity
);

// Real-time updates come automatically with WebSocket
```

## Support

For issues or questions:
1. Check application logs for errors
2. Review `README_COINBASE_WEBSOCKET_ORDERS.md`
3. Verify API credentials and permissions
4. Test with Coinbase sandbox first
