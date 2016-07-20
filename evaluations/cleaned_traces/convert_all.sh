#!/bin/bash

for file in *.rx; do
	./convert.py ${file}
	rm ${file}
done
