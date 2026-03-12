# nrf9151 AT指令测试说明

## NB-IoT卡 AT测试命令

| AT命令 | 说明 |
|--------|------|
| `AT%XSYSTEMMODE=1,1,1,1` | 配置系统模式：`AT%XSYSTEMMODE=<LTE_M_support>,<NB_IoT_support>,<GNSS_support>,<LTE_preference>` |
| `AT+CFUN=1` | 开启 modem 全功能 |
| `AT+CEREG=2` | 启用网络注册状态的非请求结果码（URC） |
| `AT+CEREG?` | 查询网络注册状态 |
| `AT+CESQ` | 查信号质量 |
| `AT#XSOCKET=1,1,0` | 创建 socket（参数含义请参考模块手册） |
| `AT#XCONNECT=0,"8.135.10.183",36628` | 创建 TCP socket 并连接到测试服务器 |
| `AT#XSEND=0,0,0,"ciallo"` | 发送数据 |
| `AT+CCLK?` | 查询实时时间 |
| `AT#XCLOSE=0` | 关闭 socket 0 |
| `AT#XCLOSE` | 关闭所有 socket |
| `AT+CIMI` | 查 IMSI（确认 SIM 运营商） |
| `AT#XGETADDRINFO="google.com"` | 解析主机名 |

## GPS/GNSS AT测试命令

| AT命令 | 说明 |
|--------|------|
| `AT%XSYSTEMMODE=1,1,1,1` | 配置系统模式 |
| `AT+CFUN=31` | 开启 GNSS 功能 |
| `AT#XGNSS=1,0,0,0` | 激活 GNSS |