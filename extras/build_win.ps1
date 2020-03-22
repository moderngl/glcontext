# Build wheels switching version with pyenv-win

# 3.5
pyenv global 3.5.4
pyenv rehash
pip install -U setuptools wheel
python setup.py bdist_wheel

pyenv global 3.5.4-amd64
pyenv rehash
pip install -U setuptools wheel
python setup.py bdist_wheel

# 3.6
pyenv global 3.6.8
pyenv rehash
pip install -U setuptools wheel
python setup.py bdist_wheel

pyenv global 3.6.8-amd64
pyenv rehash
pip install -U setuptools wheel
python setup.py bdist_wheel

# 3.7
pyenv global 3.7.6
pyenv rehash
pip install -U setuptools wheel
python setup.py bdist_wheel

pyenv global 3.7.6-amd64
pyenv rehash
pip install -U setuptools wheel
python setup.py bdist_wheel

# 3.8
pyenv global 3.8.1
pyenv rehash
pip install -U setuptools wheel
python setup.py bdist_wheel

pyenv global 3.8.1-amd64
pyenv rehash
pip install -U setuptools wheel
python setup.py bdist_wheel
