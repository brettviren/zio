VERSION='0.0.0'
APPNAME='zio'

top = '.'
out = 'build'

def options(opt):
    opt.load('compiler_cxx')
    opt.load('waf_unit_test')

def configure(cfg):
    #cfg.env.CXXFLAGS += ['-std=c++17', '-ggdb', '-O2', '-Wall', '-Wpedantic', '-Werror']
    cfg.env.CXXFLAGS += ['-std=c++17', '-ggdb3', '-Wall', '-Wpedantic', '-Werror']
    cfg.load('compiler_cxx')
    cfg.load('waf_unit_test')
    p = dict(mandatory=True, args='--cflags --libs')
    cfg.check_cfg(package='libzmq', uselib_store='ZMQ', **p);
    cfg.check_cfg(package='libczmq', uselib_store='CZMQ', **p);
    cfg.check_cfg(package='libzyre', uselib_store='ZYRE', **p);
    cfg.check_cfg(package='spdlog', uselib_store='SPDLOG', **p);

    cfg.write_config_header('config.h')
    cfg.check(features='cxx cxxprogram', lib=['pthread'], uselib_store='PTHREAD')

def build(bld):
    uses='ZMQ CZMQ ZYRE SPDLOG'.split()

    rpath = [bld.env["PREFIX"] + '/lib', bld.path.find_or_declare(bld.out_dir)]
    rpath += [bld.env["LIBPATH_%s"%u][0] for u in uses]
    rpath = list(set(rpath))
             
    sources = bld.path.ant_glob('src/*.cpp');
    bld.shlib(features='cxx', includes='inc', rpath=rpath,
              source = sources, target='zio',
              uselib_store='ZIO', use=uses)

    tsources = bld.path.ant_glob('test/test*.cpp')
    for tmain in tsources:
        bld.program(features = 'test cxx',
                    source = [tmain], target = tmain.name.replace('.cpp',''),
                    ut_cwd = bld.path,
                    install_path = None,
                    includes = 'inc build test',
                    rpath = rpath,
                    use = ['zio'] + uses + ['PTHREAD'])
    csources = bld.path.ant_glob('test/check*.cpp')
    for cmain in csources:
        bld.program(features = 'cxx',
                    source = [cmain], target = cmain.name.replace('.cpp',''),
                    ut_cwd = bld.path,
                    install_path = None,
                    includes = 'inc build test',
                    rpath = rpath,
                    use = ['zio'] + uses)

    bld.install_files('${PREFIX}/include/zio', 
                      bld.path.ant_glob("inc/zio/**/*.hpp"),
                      cwd=bld.path.find_dir('inc/zio'),
                      relative_trick=True)

    # fake pkg-config
    bld(source='libzio.pc.in', VERSION=VERSION,
        LLIBS='-lzio', REQUIRES='libczmq libzmq libzyre')
    # fake libtool
    bld(features='subst',
        source='libzio.la.in', target='libzio.la',
        **bld.env)
    bld.install_files('${PREFIX}/lib', bld.path.find_or_declare("libzio.la"))
    
    from waflib.Tools import waf_unit_test
    bld.add_post_fun(waf_unit_test.summary)
