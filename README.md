# simplefs-fuse

	Directories are not supported :)

## Setup Development environment

	sudo apt update
	sudo apt install meson ninja-build pkg-config build-essential python3-pip
	pip3 install meson --upgrade


## libfuse build 

	git submodule update --init --recursive

	cd third_party/libfuse/
	git checkout fuse-3.16.2

	meson setup build --default-library=static
	ninja -C build

## vfs build

	mkdir build && cd build
	cmake ..
	make


## Test Commands

	./build.sh

	./mkfs.vfs disk.img hello1.txt hello2.txt

	./vfs-mount disk.img /tmp/disk

	echo "Hello3" > /tmp/disk/hello3.txt

	./vfs-umount /tmp/disk

