VERSION='0.0.0'
APPNAME='zio'

top = '.'
out = 'build'

def options(opt):
    opt.load('compiler_cxx')
    opt.load('waf_unit_test')

def configure(cfg):
    cfg.env.CXXFLAGS += ['-std=c++17']
    cfg.load('compiler_cxx')
    cfg.load('waf_unit_test')
    p = dict(mandatory=True, args='--cflags --libs')
    cfg.check_cfg(package='libzmq', uselib_store='ZMQ', **p);
    cfg.check_cfg(package='libczmq', uselib_store='CZMQ', **p);
    cfg.check_cfg(package='libzyre', uselib_store='ZYRE', **p);

def build(bld):
    uses='ZMQ CZMQ ZYRE'.split()
    bld.shlib(features='cxx', includes='inc',
              source=bld.path.ant_glob('src/*.cpp'), target='zio',
              uselib_store='ZIO', use=uses)

    rpath = [bld.env["PREFIX"] + '/lib']
    rpath += [bld.env["LIBPATH_%s"%u][0] for u in uses]
    rpath = list(set(rpath))
    print (rpath)
             
    for tmain in bld.path.ant_glob('test/test*.cpp'):
        bld.program(features = 'test cxx',
                    source = [tmain], target = tmain.name.replace('.cpp',''),
                    ut_cwd = bld.path, install_path = None,
                    includes = ['inc','build','test'],
                    rpath = rpath,
                    use = uses)
