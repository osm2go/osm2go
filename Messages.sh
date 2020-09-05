xgettext --from-code=UTF-8 -L C -L C++ -k_ --flag=_1:1:pass-c-format -ktrstring --flag=trstring:1:no-c-format -ktr --flag=tr:1:no-c-format $(find src/ -name *.cpp -o -name *.c -o -name *.h)
