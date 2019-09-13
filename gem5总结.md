# 自定义调试选项的添加
    首先要在SConscript中增加DebugFlag('ckj', 'Printing the Minor related variable')
    
    然后在对应的source file中增加 #include "debug/ckj.hh"，头文件的名字一定是你增加的
    DebugFlag的名字。

# 命令
	scons build/RISCV/gem5.opt -j6
	build/RISCV/gem5.opt --debug-flags=ckj configs/example/se.py --cpu-type="MinorCPU" -c "/home/ckj/study/riscv-hello/hello32" --caches

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

# 修改思路
	首先修改ForwardInstData结构，其对应的是instruction queue，需要在该结构中增加tag来对
	instruction进行标记。本设计的主题思路就是通过可以改变control flow的指令作为分隔符来对
	instruction	queue中的指令进行着色，进而对受到推测执行影响的指令进行区分，进一步对受到推测
	执行的数据进行着色。
	
	ForwardInstData定义于pipe_data.hh中，其中该类中的insts代表的是instruction queue，
	其长度为MAX_FORWARD_INSTS，默认值为16.其中insts的类型是MinorDynInstPtr，起原始定义是
	MinorDynInst，继承于RefCounted(用来统计对类的引用计数)，同理MinorDynInstPtr也会对
	MinorDynInst类型的指针进行计数。
	
## 版本 V1.0(该方案的基本思想是不同color的code block之间不能相互影响，其问题是会造成较大的延时)
	对instruction queue要扩展一个4bit tag，用来表示16种color(0-15)，除此之外每个register都
	要额外的扩展一个4bit的tag用来表示是否受到speculation execution的影响，一旦受到推测执行的影响
	就要被标记，表示当前register被哪种color所影响。除此之外，每个Cache line也都要被分配一个4bit
	的tag，用来表示当前Cache line被哪种color所影响。
	
	在control flow发生改变时，exception，以及相同核心上切换thread时都会导致color的改变。
	
	RISC-V中设计control flow改变的指令有：JAL，JALR，BEQ，BNE，BLT，BGE，BLTU，BGEU。
	
	当register和Cache line被不同的颜色所访问时，就会产生阻塞，直到推测执行的状态确定时才能执行后续
	不同颜色的code block。而当register和Cache line被相同颜色的instruction访问时则可以继续。

## 具体的修改
	所有修改的内容都会使用/*modify_ckj*/来标识。
	
	1)fetch1的input/output：
		其中输入inp，BranchData类型，来自execute stage;
		其中输出out，ForwardLineData类型，输出到fetch2 stage;
		其中输入prediction，BranchData类型，来自fetch2 stage，对color的分配在fetch2 stage完成；

	2)fetch2的input/output：
		其中输入inp，ForwardLineData类型，来自fetch1 stage；
		其中输入branchInp，BranchData类型，来自execute stage，这其实是发送到fetch1 stage的，但是被fetch2也捕获了；
		其中输出predictionOut，BranchData类型，输出到fetch1 stage；
		其中输出out，ForwardInstData类型，输出到decode stage；
		
		这里的inputBuffer是ForwardLineData类型的。
	
	3)decode的input/output：
		其中输入inp，ForwardInstData类型，来自fetch2 stage；
		其中输出out，ForwardInstData类型，输出到execute stage;
	
	4)execute的input/output：
		其中输入inp，ForwardInstData类型，来自decode stage；
		其中输出out，BranchData类型，输出到fetch1 stage；

### fetch2.hh
#### Fetch2(class,一开始的时候该部分内容是对ForwardInstData中进行的修改，但是每个pipeline stage每次都会重新初始化该类型的数据，所以每次color都是从0开始，而不是从上次的color开始)
	
	原来的设计中还增加了s_tag，用来表示指令是否受到推测执行的影响。但是该标志位没有任何意义，因为c_tag
	就是使用可以改变控制流的instruction作为instruction queue中的分隔符，来进行颜色的区分。
	
	增加了变量maxNumColors，用来表示支持的最大color数量。
	
	增加了变量currentInstColor，用来表示当前的指令队列中的指令的color。

### fetch2.cc
	一旦返回来自execute stage的通知，即通知要改变当前的stream，就需要改变c_tag，既color++。
	
	一旦fetch2中的prediction unit做出了预测(即当前的指令是可以改变control flow的指令)，此时
	需要对color进行修改。

#### Fetch2::Fetch2
	对maxNumColors，currentInstColor进行初始化。

#### evaluate()
	增加了对指令颜色的更新。如果指令是控制指令或者sys call指令，就需要对color进行改变。除此
	之外还要考虑特权指令，比如ECALL。

	将colorTable中对应颜色的valid置为有效，对受到推测执行影响的指令的specuated置为有效。

	一旦收到来自execute stage的BranchData，如果发现stream改变，会修改当前的color。

### execute.hh
#### Execute(class)
	在该类中增加了对maxNumColors和ColorTable的定义。

	增加了DataLineTable的定义。

	增加了对register_table和maxNumRegisters的定义，其中register_table记录了当前register的颜色。通过对register的颜色的查询，进而判断当前颜色是否处于推测执行的状态，如果处于推测执行的状态，则需要禁止访问或者打印相关信息。对于x0的不做任何处理。

	添加了一个用来检测speculation风险的函数detectSpeculationRisk()。

### execute.cc
#### Execute::Execute
	增加了对maxNumColors的初始化和colorTable类型变量的空间分配。

	其中color_table中条目的valid被初始化为无效，speculated状态也被初始化为未处于推测执行状态，这是为了将第一条要被执行的指令考虑在内，因为第一条指令的颜色可能是非control-flow相关的指令，所以其可能处于非推测执行状态。

	对data_line_table进行初始化。

	对register_table进行了初始化，全部初始化为0.

#### evaluate()
	增加了对memory instruction的判断，判断访存的物理地址是否计算出来了。如果计算出来了，则需要判断当前的指令的颜色是否与受其他颜色影响的Cache line重合。

#### issue()
	在指令发射之前，首先判断当前指令是否是control flow相关的指令，如果是则在color_table中对该指令对应颜色的item的valid和speculated置为1；除此之外，对所有的要被discard的instruction，首先判断其是否是control flow相关的指令，如果是则将对应颜色的item的valid和speculated置为0.

	除了判断是否为control flow相关的指令，还要判断当前指令是否是访存指令，如果是访存指令，需要判断访存指令的地址。
	
#### commit()
	如果确定当前instruction不是要被的discard的control-flow相关指令后，就可以修改color_table中的speculated。如果当前指令是要被discard的control-flow相关指令，就要将valid和speculated置为0。

	如果指令被提交了且是control flow相关的指令，则只将speculated置为0.但是valid不用修改，因为后来的指令将会覆盖这个颜色。至少在gem5中是这样做的，但是放到hardware processor core中只需要判断instruction queue中的next instruction的颜色是不是与当前指令的颜色相同，则将color_table中的该颜色的valid置为0.


### dyn_inst.hh
#### MinorDynInst(class)
	增加了变量colorTag，用来标识当前instruction的颜色。
	用来标识instruction queue(insts)中的instruction的状态，colorTag(代
	表当前指令的颜色，其长度为4 bit，共可以表示16种颜色，范围0-15，其中0代表初始颜色，system boot时，第一条被执行的指令被标记为color-0,当遇到第一条改变control flow的指令时才会改变颜色，此时该指令需要等待被标记为color-0的指令提交后才能执行，这里对应的是gem5 Minor中
	instruction queue中可以放16条指令，这样不会存在color不够用的情况)。不同颜色的分配是根据当前指
	令是否可以改变PC进行分配的，比如call-return，conditional branch，indirect branch，direct
	branch。除了上述可以改变PC的指令外，异常也要被考虑在内。

	增加了对指令是否是load指令或store指令的判断。

	增加对是否有source register和destination register的判断标志。
	增加对source register和destination register的访问是否合法的判断标志。

	为每条指令分配一个lastTag，用来判断当前指令是否是该颜色中的最后一条指令。当该指令被提交后，当前颜色就会处于无效状态。	