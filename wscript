VERSION='0.0.0'
APPNAME='zio'

top = '.'
out = 'build'

def options(opt):
    opt.load('compiler_cxx')
    opt.load('waf_unit_test')

def configure(cfg):
    cfg.env.CXXFLAGS += ['-std=c++17', '-g', '-O2']
    cfg.load('compiler_cxx')
    cfg.load('waf_unit_test')
    p = dict(mandatory=True, args='--cflags --libs')
    cfg.check_cfg(package='libzmq', uselib_store='ZMQ', **p);
    cfg.check_cfg(package='libczmq', uselib_store='CZMQ', **p);
    cfg.check_cfg(package='libzyre', uselib_store='ZYRE', **p);
    cfg.check_cfg(package='protobuf', uselib_store='PROTOBUF', **p);
    cfg.write_config_header('config.h')


def build(bld):
    uses='ZMQ CZMQ ZYRE'.split()

    rpath = [bld.env["PREFIX"] + '/lib', bld.path.find_or_declare(bld.out_dir)]
    rpath += [bld.env["LIBPATH_%s"%u][0] for u in uses]
    rpath = list(set(rpath))
    print ('\n'.join([str(p) for p in rpath]))
             
    bld.shlib(features='cxx', includes='inc', rpath=rpath,
              source=bld.path.ant_glob('src/*.cpp'), target='zio',
              uselib_store='ZIO', use=uses)

    for tmain in bld.path.ant_glob('test/test*.cpp'):
        bld.program(features = 'test cxx',
                    source = [tmain], target = tmain.name.replace('.cpp',''),
                    ut_cwd = bld.path, install_path = None,
                    includes = ['inc','build','test'],
                    rpath = rpath,
                    use = ['zio'] + uses)

    bld.install_files('${PREFIX}/include/zio', bld.path.ant_glob("inc/zio/*.hpp"))

    # fake pkg-config
    bld(source='zio.pc.in', VERSION=VERSION,
        LLIBS='-lzio', REQUIRES='libczmq libzmq libzyre')
    # fake libtool
    bld(features='subst',
        source='libzio.la.in', target='libzio.la',
        **bld.env)
    bld.install_files('${PREFIX}/lib', bld.path.find_or_declare("libzio.la"))
