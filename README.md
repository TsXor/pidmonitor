# PIDMonitor  
利用WMI和ETW获取指定程序映像的实时PID。  

## 使用  
`cmake`（源码tarball和二进制包均可）  
```cmake
add_subdirectory(path/to/pidmonitor pidmonitor.out EXCLUDE_FROM_ALL)
target_link_libraries(your_target pidmonitor)
target_include_directories(your_target PRIVATE path/to/pidmonitor/include)
```

## 使用非公开的ETW接口
定义宏`__TRACE_USE_UNDOCUMENTED__`。  
注意：此接口取自[TraceEvent](https://github.com/Biswa96/TraceEvent)，使用后请遵守GPL协议。  

## 测试
```bash
bin\pidmon_test.exe firefox.exe
```
