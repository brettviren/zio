import setuptools

setuptools.setup(
    name="zio",
    version="0.0.0",
    author="Brett Viren",
    author_email="brett.viren@gmail.com",
    description="A simplified, high level C++ interface to ZeroMQ and Zyre",
    url="https://brettviren.github.io/zio",
    packages=setuptools.find_packages(),
    python_requires='>=3.3',    # how to know?
    install_requires = [
        "pyzmq"
    ]
)

