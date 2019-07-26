# 自定义调试选项的添加
    首先要在SConscript中增加DebugFlag('ckj', 'Printing the Minor related variable')
    
    然后在对应的source file中增加 #include "debug/ckj.hh"，头文件的名字一定是你增加的
    DebugFlag的名字。

# InstID

## Fetch1
	InstId::threadId 代表的是thread number，由Fetch1产生。
	InstId::lineSeqNum 代表的是从Fetch1中取出的Cache line sequence number。
	
## Fetch2
	InstId::predictionSeqNum 代表的是branch prediction decision，Fetch2可以根据
	最近的branch prediction来标记line和instruction。Fetch2通知Fetch1改变fetch address，
	同时将line标记为新的prediction sequence number。
	
	InstId::fetchSeqNum 代表的是指令的fetch order(将Cache line分解成多个指令)。
	
## Decode
	InstId::execSeqNum 代表的是micro-op 分解后的instruction order。

## Execute
	InstId::streamSeqNum 代表的是在execute stage选择的stream sequence number，PC的
	改变会导致streamSeqNum的改变。PC的改变和streamSeqNum的改变都发生在Execute stage中。
	由于branch和exception造成的。

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

# Fetch2

## evaluate()中的重要函数和变量
	首先要判断来自execute stage中有关control flow的相关信息。在判断完这些信息后才
	会分析来自Fetch1的指令。

### insts_out && branch_inp
    insts_out代表的是要发送给decode stage的指令。
    branch_inp代表的是发送给fetch1 stage的分支预测。

### updateBranchPrediction()
	该函数是针对来execute stage的信息进行判断的。

    对推测执行部件的更新。再该函数中用到了suqash和update(该函数了重写了bpred_unit.hh
    中的纯虚函数update())两个函数。
    该函数使用到了bpred_unit.hh和bpred_unit.cc中的分支预测器。这两个文件在
    (src/cpu/pred)中。

    Minor预测了分支是否发生，以及branch target，一旦这两者中有一个预测错误了就会调用
    BPredUnit::squash()来撤销所有的分支。具体发生在如下两种场景中：
    1)在分支执行过后，对ROB中的状态进行了更新，在commit stage检查ROB的更新，同时向fetch
    stage中发送信号来squash 错误预测的history。
    2)在decode stage中发现前面的unconditional，PC-relative分支(即间接分支)预测错了，这
    时候会通知fetch stage进行squash。

    bpred_unit.hh中定义了一个结构体PredictorHistory，其中记录了有关分支预测的相关信息，这
    些信息可以用来更新BP，BTB，RAS(Return Address Stack，相当于RSB)。比如指令的类型(call，
    return，indirect branch)，预测的情况(预测是否发生，是否使用RAS)。

# predictBranch()
	用来对当前的控制指令或系统调用指令(刚从Cache line中取出，并且已经译码结束)进行分支预测。
	
	调用了bpred_unit.cc中的predict()函数完成实际的预测工作。其中的uncondBranch()是一个
	纯虚函数，在不同的分支预测器中其实现不同。
	在predict()中，先判断分支是否发生(非条件分支一定发生，条件分支要通过对Branch Predictor
	进行查找，调用lookup())，再判断target(从BTB或RAS中查找target)。
