# SecFS

一个简单的二级文件系统，基于xv6，实现文件系统的基本功能，提供了基本的文件操作接口。

## 使用说明

工作平台:
    
    Linux

工作条件:

    安装 `gcc` 编译器

编译secfs:

    $ make clean && make    

复制所需文件:

    $ make import

执行secfs:
    
    $ cd build
    $ ./secfs

## 可用命令

列出目录下文件

    $ ls

切换目录

    $ cd home

创建文件夹

    $ mkdir test

建立文件

    $ touch foo

删除文件/文件夹

    $ del foo

查看文件

    $ cat foo

导入文件

    $ import rabbit.gif

测试文件指针

    $ cat Jerry
    $ testfeek
    $ cat Jerry

退出文件系统

    $ exit


## 文件映像

使用如下指令查看文件映像十六进制形式

    $ hexdump -C fs.img

