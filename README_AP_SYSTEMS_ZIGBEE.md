
## Todos

- IEEE is hardcoded to 0xD8A3011B9780
- Unable to change channel (use firmware with channel 16 as default)

## Already included

- AP System profile and Endpoint registered
- IEEE adress set
- Excluded security stuff if firmware is without SECURE compiled

### Configure
```
ZbConfig {"Channel":16,"PanID":"0xA3D8","ExtPanID":"0xD8A3FFFF823D0910"}
```

### Send to broadcast to all inverters
```
ZbZNPSend 2401FFFF1414060000000F1E80971B01A3D8FBFB1100000D6030FBD3000000000000000004010281FEFE
```

### Send request to inverter 0x3D82
```
ZbZNPSend 2401823D1414060001000F1380971B01A3D8FBFB06BB000000000000C1FEFE
```

