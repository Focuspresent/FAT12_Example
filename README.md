## FAT12的命令实现
### 实现的功能
- 输出当前目录下的文件和目录
- 切换目录
- 创建目录/文件
- 删除目录/文件
- 查看文件内容
- 查看16进制磁盘数据
### 前提概要
本程序基于某大佬的框架实现，所以本篇不对底层函数封装做过多的解释，只会讲解几个命令的实现以及如何调用，若是对底层框架感兴趣，详情可见
[FAT12的框架实现](https://github.com/ThousandPine/FAT12_Test)
### 系统实现的基本思路
本程序基于**win系统**实现，底层调用win自带函数，通过操作句柄来实现对磁盘数据的读取和写入，其中最为重要的是**偏移量**。我们首先需要读取引导扇区的数据，来获得几个重要的数据，如保留扇区，簇占的扇区数，以及fat表占用的扇区数等，通过这些数据我们可以得到fat表，根目录，数据区的起始偏移量。并且我们先将大部分数据限制在**一个簇**里，并对其中的数据进行操作，然后在命令函数时，我们可以**多次调用**这些**单簇操作函数**，并且下列涉及到的文件和目录均是**短文件条目**。
### ls
**单簇**情况下，我们需要计算每簇存放文件条目的上限，并且从某簇的**起始偏移**开始读取，然后依次将数据读取并存放在结构体中，按照条件筛选数据，并按格式输出即可，特别注意的是虽然磁盘存放的数据条目中的文件和扩展名**均是大写**，但可以通过文件条目的**第一个保留位**来进行输出的优化，**多簇**情况下，我们可以多次传入不同簇的起始偏移
### cd
对于cd的命令，只是一个简单的实现，我们用一个**数组**来记录当前**目录的偏移量**，以及用一个计数器来记录当前的目录位置，并且，切换目录时，同样需要更新**多簇的数组**和**多簇的计数器**，多簇的数组存放的是不同簇的偏移量，多簇计数器是记录有多少个簇，总实现四种情况，分为返回上一级，返回当前目录，返回根目录以及进入下一级目录
### mkdir
创建目录时，我们需要从当前的偏移量找到空闲位置，若**当前簇已经被填充**，我们需要寻找**第一个空闲的簇号**，并修改**fat表**和**多簇信息**，并判断该目录**是否已经存在**(不分大小写)，若不存在，则继续创建，并且在创建目录时，我们也需要分配给其一个空闲的簇号，并在该簇的数据里写入上一级目录和当前目录，也就是点和点点
### touch
创建文件时，我们需要从当前的偏移量找到空闲位置，若**当前簇已经被填充**，我们需要寻找**第一个空闲的簇号**，并修改**fat表**和**多簇信息**，并判断该文件**是否已经存在**(不分大小写)，若不存在，则继续创建，但与创建目录不同的是，不需要分配簇号
### rm
分为删除文件和删除目录。在**删除文件**时，假如文件是**多簇存储**，我们需要修改fat表(删除时不清空簇的数据,重新被分配才清空旧簇的数据)，已经在win上，只需将文件条目的首个数据置为**占用字节**即可；在**删除目录**时，假如当前目录下还有子目录，需要**递归删除**所有的目录以及文件，删除目录时，也需要修改fat表
### cat
查看文件内容，我们需要**一个簇一个簇**的将文件数据打印到屏显中，知道文件的下一个簇号是结束簇号位置，并且只打印**非空数据**，所以我们需要通过读取fat表来得到下一个簇号信息，以此循环直到结束簇号
### hexdump 
只实现了简单的查看16进制的磁盘数据，但也需要加上-C选项，以及后面的起始偏移(16进制)和字节上限(16的倍数)，人为规定16字节输出一行