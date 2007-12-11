import os
import os.path
import SCons.Warnings

class JavaNotFound(SCons.Warnings.Warning):
    pass
SCons.Warnings.enableWarningClass(JavaNotFound)

def generate(env):
    path = os.environ.get('JAVA_HOME',None)
    if not path:
       SCons.Warnings.warn(JavaNotFound,"Could not detect Java")
    else:
       env.AppendUnique(CXXFLAGS="-I%s/include -I%s/include/solaris" % (path,path),
                        LIBPATH=['%s/jre/lib/sparcv9' % (path), '%s/jre/lib/sparcv9/server' % (path)],
                        LIBS=["java", "jvm"])

def exists(env):
    return env.Detect('java')
