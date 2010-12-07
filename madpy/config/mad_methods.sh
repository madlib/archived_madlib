#!/bin/sh
which python
python -c 'from madpy.config import mad_methods;mad_methods.install_methods()'
