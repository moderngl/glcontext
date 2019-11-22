#!/bin/bash
python3.5 setup.py bdist_wheel
python3.5 -m pip install -U setuptools wheel

python3.6 setup.py bdist_wheel
python3.6 -m pip install -U setuptools wheel

python3.7 setup.py bdist_wheel
python3.7 -m pip install -U setuptools wheel

python3.8 setup.py bdist_wheel
python3.8 -m pip install -U setuptools wheel
