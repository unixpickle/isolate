build/isolate: main.c
	mkdir -p build
	gcc -o build/isolate main.c

clean:
	rm -rf build