#!/bin/sh
which python
python -c 'from madpy.config import mkmethods;mkmethods.install_methods()'
