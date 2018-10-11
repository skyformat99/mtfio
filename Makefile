build:
	gcc -c ./src/mtfio.c -o mtfio.o
	ar rc libmtfio.a mtfio.o
	ranlib libmtfio.a
	cp ./src/mtfio.h ./
	rm ./mtfio.o
	mv *bmtf* ../cliaf/bin/static
	mv *mtf* ../cliaf/bin


