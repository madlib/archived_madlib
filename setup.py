from distutils.core import setup, Extension
methods = ['sketch','profile','bayes']
suffixes = ['c', 'h', 'in', 'yml', 'sh']
additional_files = ['Makefile']
ports = {'default': 'extended_sql/pg_gp', 'profile': 'simple_sql'}


pkg_data = {'madlib' : []}
for m in methods:
    if m in ports:
        pport = ports[m]
    else:
        pport = ports['default']
    for s in suffixes:
        # print "pkg_data['madlib'] += " + str([m+'/src/'+pport+'/*.'+s])
        pkg_data['madlib'] += [m+'/src/'+pport+'/*.'+s]
    pkg_data['madlib'] += [m+'/src/'+pport+'/'+ f for f in additional_files]
    
pkg_data['madpy'] = []
for s in suffixes:
    pkg_data['madpy'] += ['config'+'/*.'+s]
    
setup(name='Madlib',
      version='0.01',
      description='MADlib library of SQL analytics',
      url='http://github.com/madlib',
      packages=['madpy','madlib','madpy.config','madpy.migrate'],
      package_dir={'madpy': 'madpy', 'madlib': 'methods'},
      package_data=pkg_data,
      requires=['yaml','os','argparse', 'shutil', 'sqlparse', 'psycopg2', 'imp', 'traceback', 'hashlib'],
      provides=['madpy','madlib'],
      scripts=['madpy/config/mad_install.py']
      )
