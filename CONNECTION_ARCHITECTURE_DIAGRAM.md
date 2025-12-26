# Connection Architecture Diagram

## Class Hierarchy

```
┌─────────────────────────────────────────────────────────────┐
│                      IConnection                             │
│  (Interface: Connect, Disconnect, IsConnected, etc.)         │
└──────────────────────────┬──────────────────────────────────┘
                           │
┌──────────────────────────┴──────────────────────────────────┐
│                   ConnectionBase                             │
│  ┌────────────────────────────────────────────────────────┐ │
│  │ Common WebSocket Infrastructure                        │ │
│  │                                                         │ │
│  │ • WebSocket lifecycle (Connect/Disconnect)             │ │
│  │ • Message sending/receiving                            │ │
│  │ • Message processor                                    │ │
│  │ • Settings & Logger                                    │ │
│  │ • Instruments management                               │ │
│  │ • Symbol translation                                   │ │
│  └────────────────────────────────────────────────────────┘ │
└──────────────┬─────────────────────────┬────────────────────┘
               │                         │
       ┌───────┴───────┐         ┌──────┴────────┐
       │               │         │               │
       ▼               │         ▼               │
┌─────────────────┐    │  ┌─────────────────┐   │
│ConnectionBaseMD │    │  │ConnectionBaseORD│   │
│                 │    │  │                 │   │
│ Market Data     │    │  │ Order Mgmt      │   │
│ Specialized     │    │  │ Specialized     │   │
│                 │    │  │                 │   │
│ • PublishQuotes │    │  │ • Start()       │   │
│ • ParseQuote    │    │  │ • Subscribe()   │   │
│ • Subscribe     │    │  │ • Unsubscribe() │   │
│ • Snapshot      │    │  │                 │   │
│ • Depth         │    │  │ (Extensible)    │   │
│                 │    │  │                 │   │
│ ActiveQuoteTable│    │  │                 │   │
└────────┬────────┘    │  └────────┬────────┘   │
         │             │           │            │
    ┌────┴────┬────────┤      ┌────┴───────┐    │
    │         │        │      │            │    │
    ▼         ▼        ▼      ▼            │    │
┌────────┐ ┌────────┐ ┌────────┐ ┌──────────────┴──────┐
│BINANCE │ │COINBASE│ │  OKX   │ │   COINBASE          │
│   MD   │ │   MD   │ │   MD   │ │   ConnectionORDWS   │
└────────┘ └────────┘ └────────┘ └─────────────────────┘
```

## Responsibilities

### ConnectionBase (Common Infrastructure)
```
┌────────────────────────────────────────┐
│         ConnectionBase                 │
├────────────────────────────────────────┤
│ WebSocket Management:                  │
│  • CreateWebSocket()                   │
│  • ReceiveWebSocketData()              │
│  • Send()                              │
│                                        │
│ Connection Lifecycle:                  │
│  • Connect()                           │
│  • Disconnect()                        │
│  • IsConnected()                       │
│                                        │
│ Core Services:                         │
│  • MessageProcessor                    │
│  • Logger                              │
│  • Settings                            │
│  • GetInstruments()                    │
│  • TranslateSymbol()                   │
└────────────────────────────────────────┘
```

### ConnectionBaseMD (Market Data)
```
┌────────────────────────────────────────┐
│       ConnectionBaseMD                 │
├────────────────────────────────────────┤
│ Market Data Specific:                  │
│  • PublishQuotes()                     │
│  • ParseQuote()                        │
│  • SubscribeInstrument()               │
│  • UnsubscribeInstrument()             │
│  • Snapshot()                          │
│  • GetDepth()                          │
│                                        │
│ Quote Management:                      │
│  • SideTranslator()                    │
│  • ParseMessage()                      │
│  • PublishQuote()                      │
│                                        │
│ State:                                 │
│  • ActiveQuoteTable                    │
│  • PublishedQuotesCounter              │
└────────────────────────────────────────┘
```

### ConnectionBaseORD (Order Management)
```
┌────────────────────────────────────────┐
│       ConnectionBaseORD                │
├────────────────────────────────────────┤
│ Order Management Specific:             │
│  • Start() - Subscribe to orders       │
│  • Subscribe() - Order updates         │
│  • Unsubscribe()                       │
│                                        │
│ Future Extensions:                     │
│  • SendOrder() [virtual]               │
│  • CancelOrder() [virtual]             │
│  • GetOrders() [virtual]               │
│  • TranslateExecutionReport()          │
│                                        │
│ State:                                 │
│  • [To be added as needed]             │
└────────────────────────────────────────┘
```

## Data Flow

### Market Data Flow
```
Exchange WebSocket
      │
      ▼
ConnectionBase::ReceiveWebSocketData()
      │
      ▼
ConnectionBase::MessageProcessor
      │
      ▼
ConnectionMD::OnMessage() [Exchange-specific]
      │
      ▼
ConnectionBaseMD::ParseQuote()
      │
      ▼
ConnectionBaseMD::PublishQuotes()
      │
      ▼
OrderBook
```

### Order Management Flow (WebSocket)
```
Your Code
      │
      ▼
ConnectionORDWS::SendOrder()
      │
      ▼
ConnectionBase::Send() [JWT-signed WebSocket message]
      │
      ▼
Exchange
      │
      ▼
ConnectionBase::ReceiveWebSocketData()
      │
      ▼
ConnectionORDWS::OnOrderUpdate()
      │
      ▼
ExecutionReportData
      │
      ▼
Your System
```

## Inheritance Chain Examples

### Binance Market Data
```
IConnection
    └── ConnectionBase (Common WebSocket)
            └── ConnectionBaseMD (Market Data Specific)
                    └── BINANCE::ConnectionMD (Binance Specific)
```

### Coinbase WebSocket Orders
```
IConnection
    └── ConnectionBase (Common WebSocket)
            └── ConnectionBaseORD (Order Management Specific)
                    └── COINBASE::ConnectionORDWS (Coinbase WS Orders)
```

## Benefits Visualization

```
Before Refactoring:
┌─────────────────────────────────┐
│      ConnectionBase             │
│  (Market Data + Orders Mixed)   │
│                                 │
│  ❌ Tight coupling              │
│  ❌ Hard to extend              │
│  ❌ Confusing responsibilities  │
└─────────────────────────────────┘

After Refactoring:
         ┌──────────────────┐
         │ ConnectionBase   │
         │  (Common Only)   │
         └────────┬─────────┘
                  │
        ┌─────────┴─────────┐
        │                   │
┌───────▼────────┐  ┌───────▼────────┐
│ConnectionBaseMD│  │ConnectionBaseORD│
│ (MD Specific)  │  │ (ORD Specific) │
└────────────────┘  └────────────────┘

✅ Clear separation
✅ Easy to extend
✅ Single responsibility
✅ Type safe
```

## File Organization

```
include/
  ├── ConnectionBase.h        (Common WebSocket infrastructure)
  ├── ConnectionBaseMD.h      (Market data specialization)
  ├── ConnectionBaseORD.h     (Order management specialization)
  ├── binance/
  │   └── ConnectionMD.h      (inherits ConnectionBaseMD)
  ├── coinbase/
  │   ├── ConnectionMD.h      (inherits ConnectionBaseMD)
  │   └── ConnectionORDWS.h   (inherits ConnectionBaseORD)
  └── OKX/
      └── ConnectionMD.h      (inherits ConnectionBaseMD)

src/
  ├── ConnectionBase.cpp      (Common implementation)
  ├── ConnectionBaseMD.cpp    (MD implementation)
  ├── ConnectionBaseORD.cpp   (ORD implementation)
  ├── binance/
  │   └── ConnectionMD.cpp
  ├── coinbase/
  │   ├── ConnectionMD.cpp
  │   └── ConnectionORDWS.cpp
  └── OKX/
      └── ConnectionMD.cpp
```
