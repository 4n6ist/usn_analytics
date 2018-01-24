# USN Analytics

## Build

```
git clone https://github.com/4n6ist/usn_analytics.git
```

### Linux

```
sudo dnf groupinstall development-tools // Fedora
sudo dnf install glibc-static libstdc++-static // Fedora
sudo apt-get install build-essential // Debian/Ubuntu
cd usn_analytics
make
```

### Windows

Install MinGW-W64 (https://sourceforge.net/projects/mingw-w64/)

```
cd usn_analytics
C:\Program Files\mingw-w64\...\mingw64\bin\mingw32-make.exe
```

### macOS

macOS(OS X) doesn't support static binary build so edit Makefile

```
#CFLAGS := -std=gnu++11 -O3 -static
CFLAGS := -std=gnu++11 -O3
```

then cd usn_analtyics ; make

## Documentation & Download

Documentation and binaries are available at https://www.kazamiya.net/usn_analytics/

