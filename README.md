# PIDMonitor  
利用WMI和ETW获取指定程序映像的实时PID。  

## 使用  
`cmake`（源码tarball和二进制包均可）  
```cmake
add_subdirectory(path/to/pidmonitor pidmonitor.out EXCLUDE_FROM_ALL)
target_link_libraries(your_target name2pid)
target_link_libraries(your_target tracepid)
target_include_directories(your_target PRIVATE path/to/pidmonitor/include)
```


## 测试
```bash
bin\pidmon_test.exe firefox.exe
```
