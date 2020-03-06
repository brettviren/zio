import setuptools

setuptools.setup(
    name="zio",
    version="0.0.0",
    author="Brett Viren",
    author_email="brett.viren@gmail.com",
    description="A high level C++ interface to ZeroMQ and Zyre",
    url="https://brettviren.github.io/zio",
    packages=setuptools.find_packages(),
    python_requires='>=3.3',    # how to know?
    install_requires = [
        "click",
        "pyzmq",                # note: must install first manually to get "draft" sockets!
        "pyre==0.3.2",          # note: see requirements.txt
        "h5py",
        "rule==0.1.2bv",        # note: see requirements.txt
        "pyparsing",
        "jsonnet",
    ],
    entry_points = dict(
        console_scripts = [
            'zio = zio.__main__:main',
        ]
    ),
)

