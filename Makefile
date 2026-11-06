run:
	@cmake -S . -B build && cd build && make -j8 && ./renderer

compile:
	@cmake -S . -B build && cd build && make -j8

fmt:
	@find src/ -iname '*.hh' -o -iname '*.cc' | xargs clang-format -i
	@find assets/shaders/ -iname '*.vert' -o -iname '*.frag' | xargs clang-format -i

