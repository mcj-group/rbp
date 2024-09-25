from __future__ import print_function

from SCons import Warnings

Import('env','runtime')

def CheckJava(context):
    context.Message('Checking for java compiler... ')
    ret = context.TryAction(action='javac --version')
    context.Result(ret[0])
    return ret[0]

if runtime == "competition":
    javaEnv = Environment()
    conf = javaEnv.Configure(custom_tests={'CheckJava': CheckJava})
    hasJava = conf.CheckJava()
    javaEnv = conf.Finish()
    if hasJava:
        javaEnv.Java('classes', 'src')
    else:
        Warnings.warn(Warnings.WarningOnByDefault,
                      "relaxed-bp not built. "
                      "Try apt-get install default-jdk-headless")

    # Exclude Swarm objects
    sources = Glob('./cpp/*.cpp',
                   exclude=['./cpp/*swarm*.cpp',
                            './cpp/*test.cpp'])
    env.Append(LIBS = ['pthread'])
    env.Program('bp', source=sources)

    # TODO(mcj) consider reorganizing this as a unit test
    mqEnv = env.Clone()
    mqEnv.Program('mqtest', source='./cpp/multiqueue_test.cpp')
else:
    sources = Glob('./cpp/*.cpp', exclude='./cpp/*test.cpp')
    env.Program('bp', source=sources)
