OUT = score
CC := clang
CXX := clang++
TARGET := x86_64-pc-windows-gnu
SRC = src/dllmain.cpp src/SigScan.cpp src/helpers.cpp tomlc99/toml.c minhook/src/buffer.c minhook/src/hook.c minhook/src/trampoline.c minhook/src/hde/hde32.c minhook/src/hde/hde64.c imgui/imgui.cpp imgui/imgui_demo.cpp imgui/imgui_draw.cpp imgui/imgui_tables.cpp imgui/imgui_widgets.cpp imgui/backends/imgui_impl_dx11.cpp imgui/backends/imgui_impl_win32.cpp
OBJ = ${addprefix ${TARGET}/,${subst .c,.o,${SRC:.cpp=.o}}}
CXXFLAGS = -std=c++17 -Iminhook/include -Iimgui -Iimgui/backends -Itomlc99 -Wall -Ofast -target ${TARGET} -DWIN32_LEAN_AND_MEAN -D_WIN32_WINNT=_WIN32_WINNT_WIN7
CFLAGS = -std=c99 -Iminhook/include -Iimgui -Iimgui/backends -Itomlc99 -Wall -Ofast -target ${TARGET} -DWIN32_LEAN_AND_MEAN -D_WIN32_WINNT=_WIN32_WINNT_WIN7
LDFLAGS := -shared -static -static-libgcc -s -pthread -lgdi32 -ldwmapi -ld3dcompiler

all: options ${OUT}

.PHONY: options
options:
	@echo "CXXFLAGS	= ${CXXFLAGS}"
	@echo "LDFLAGS	= ${LDFLAGS}"
	@echo "CXX	= ${CXX}"

.PHONY: dirs
dirs:
	@mkdir -p ${TARGET}/src
	@mkdir -p ${TARGET}/minhook/src/hde
	@mkdir -p ${TARGET}/imgui/backends
	@mkdir -p ${TARGET}/tomlc99

${TARGET}/%.o: %.cpp
	@echo BUILD $@
	@${CXX} -c ${CXXFLAGS} $< -o $@

${TARGET}/%.o: %.c
	@echo BUILD $@
	@${CC} -c ${CFLAGS} $< -o $@

.PHONY: ${OUT}
${OUT}: dirs ${OBJ}
	@echo LINK $@
	@${CXX} ${CXXFLAGS} -o ${TARGET}/$@.dll ${OBJ} ${LDFLAGS} ${LIBS}

.PHONY: fmt
fmt:
	@cd src && clang-format -i *.h *.cpp -style=file

.PHONY: clean
clean:
	rm -rf ${TARGET}
