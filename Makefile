CC := g++
# for Static Binary (Windows/Linux)
CFLAGS := -std=gnu++11 -O3 -static
# for not Static Binary (macOS)
#CFLAGS := -std=gnu++11 -O3
INCLUDE := -I./include/
LIBS := lib/*.cpp
SRCS := src/*.cpp
INCLUDES := include/*.h

all: $(SRCS) $(INCLUDES) $(LIBS)
	$(CC) $(CFLAGS) $(INCLUDE) $(LIBS) $(SRCS) -o usn_analytics
