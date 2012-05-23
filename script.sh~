#! /bin/bash
echo "inicio"
gcc -o cliente cliente.c

for i in {1..50}
do
  ./cliente < input.txt &
done

echo "fim"
