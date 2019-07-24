# 自定义调试选项的添加
    首先要在SConscript中增加DebugFlag('ckj', 'Printing the Minor related variable')
    
    然后在对应的source file中增加 #include "debug/ckj.hh"，头文件的名字一定是你增加的
    DebugFlag的名字。

# Fetch1

## evaluate()中的重要函数和变量

### fetch2_branch && execute_branch
    execute_branch代表的是来自execute stage的branch结果，这个代表的是真正的branch的结果。
    fetch2_branch代表的是来自fetch2 stage的分支预测的结果。
    如果同时收到两者，execute_branch的优先级更高。

    通过fetch2_branch.isStreamChange()进行的判断，都要注意，总共有两个，这两都会根据预测
    结果修改要预取的指令的地址。都会将Fetch1ThreadInfo::pc改为预测的target，这里的pc并不
    是个地址，而是一个类(TheISA::PCState)的实例化。指令的地址保存在pc.instAddr()。
    
    Fetch1ThreadInfo::pc会在fetchLine()中被取出。其中pc的类型是TheISA::PCState，这个
    依赖于使用的ISA类型，比如RISC-V，其定义在src/arch/riscv/types.hh。

### fetchLine()
    fetchLine()会完成预取指令所在的Cache line。aligned_pc是指令所在的Cache line的虚拟
    地址，通过调用translateTiming将虚拟地址翻译成物理地址。

    translateTiming中调用了finish()，finish()是ITLB响应的接口。

    而finish()中调用了translateAtomic()，实现基本的TLB，该实现依赖于所使用的ISA，对于
    RISC-V，其实现在于src/arch/riscv/tlb.cc中，translateInst()完成对指令地址的转换，
    translateData()完成对数据地址的转换。这两个函数都会调用src/mem/page_table.cc中的
    translate()来完成实际的翻译过程，同时其物理地址通过调用setPaddr()来完成设置。

### stepQueues()
    fetch1.hh中有两个FetchQueue，一个是请求队列(requests，请求对虚拟地址的翻译)，另一个
    是响应队列(transfers，对内存的请求和响应)。

    stepQueues()将requests队列中的请求移到transfers。

    processResponse()将transfers队列中的response转换为ForwardLineData。
    
    adoptPacketData()将packet中的数据取出放到ForwardLineData中。