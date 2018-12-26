# kvm
Just to implement a kvm 
## 1.检查主机是否支持KVM

- $ egrep '(vmx|svm)' /proc/cpuinfo
  
- 如果主机本身就在虚拟化的环境中，可能不支持虚拟化(不支持虚拟化嵌套),比如在云服务器中不支持虚拟化
  
 ## 2.安装 KVM 设备
    
 - 首先检查KVM设备在本机是否存在
  
- $ ls -al /dev/kvm
  
- 如果不存在，需要安装 KVM 环境(例如 ubuntu环境)
   
- $ sudo apt install qemu-kvm 
  
 ## 3. 如果不是root用户，需要把用户添加到kvm组
 
 - $ sudo usermod -a -G kvm `whoami`
 
 
 
## 4. 运行
- git clone https://github.com/x-lugoo/kvm.git
- make 
- make run
