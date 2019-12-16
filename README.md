## CSE-lab
#### lab1
这个lab比较简单，当前的代码是bug-free的。
#### lab2
这个代码是过了测试的，但我做测试的时候发现它是有点问题的。在高并发的情况下，yfs层的锁并不能保证最底下的inode层的正确性，导致两个inode可能会指向同一个disk number（因为分配disk block的时候没有加锁）。所以，为了改进，在lab3里面我给disk层也加了锁。<p style="color:red">lab2里相应的代码我还没改，你可以参考lab3里相应部分的代码。</p>
#### lab3
这个主要是一个protocol的设计。
#### lab4
这个是另一个protocol的设计，我的设计方式和lab3基本一模一样。最后rpc只有800多，效果很不错。