import madpy.madpack.configyml
from distutils.core import setup, Extension

suffixes = ['c', 'h', 'py', 'in', 'yml', 'sh', 'sql', 'mk']
additional_files = ['Makefile']

conf = madpy.madpack.configyml.get_config('madpy', True)
rev = madpy.madpack.configyml.get_version('madpy')

pkg_data = {'madlib' : []}
for mpair in conf['methods']:
    m = mpair['name']
    pport = mpair['port']
    for s in suffixes:
        pkg_data['madlib'] += [m+'/src/'+pport+'/*.'+s]
        pkg_data['madlib'] += [m+'/src/'+pport+'/sql/*.'+s]
    pkg_data['madlib'] += [m+'/src/'+pport+'/'+ f for f in additional_files]
    pkg_data['madlib'] += [m+'/src/'+pport+'/sql/*.sql']
    pkg_data['madlib'] += [m+'/src/'+pport+'/expected/*.out']

pkg_data['madpy'] = []
for s in suffixes:
    pkg_data['madpy'] += ['*.' + s]
    pkg_data['madpy'] += ['config'+'/*.'+s]
    pkg_data['madpy'] += ['ext'+'/*.'+s]
    
setup(name='madlib',
      version=rev,
      author="The MADlib project",
      author_email="support@madlibrary.org",
      description='MADlib library of SQL analytics',
      url='http://github.com/madlib',
      packages=['madpy','madlib','madpy.config','madpy.madpack', 'madpy.ext'],
      package_dir={'madpy': 'madpy', 'madlib': 'methods'},
      package_data=pkg_data,
      scripts=['madpy/madpack/madpack'],
      requires=['yaml','argparse', 'shutil', 'sqlparse', 'imp', 'traceback', 'hashlib'],
      provides=['madpy','madlib']
      )
