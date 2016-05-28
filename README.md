#Implementation of a MIPS simulator consisting of an assembler / dissambler, a pipelined CPU, a hierarchical memory system that support a subset of the MIPS ISA.<br /> 
1. Memory system (C++) <br />
    (a)Tunable multi-way associative cache with hierarchical structure. <br />
	(b)Random / LRU eviction. <br />
	(c)Write back / through, allocate policy <br />
2. CPU (C++)
	(a) Five stages pipline
	(b) Full ISA support (see the report form instruction details)i
3. Assembler / disassembler
	(a)A 2-pass Assembler to translate assembly language into binary machine code
	(b)A dissemlber to translate binary machine code into assembly language
4. Benchmark programs: (Assembly)
	(a) Exchange sort
	(b) Matrix multiplication
	(c) Vectorized matrix multiplication
5. User interface (Qt,C++)

