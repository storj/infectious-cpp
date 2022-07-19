import os

env = Environment(
    CXX=os.environ.get('CXX', 'clang++'),
    CXXFLAGS='-std=c++20',
    CCFLAGS='-g -Wall -Werror -Wextra -pedantic',
)
env['ENV']['TERM'] = os.environ.get('TERM', 'dumb')
Export('env')

SConscript(['lib/SConscript'])
