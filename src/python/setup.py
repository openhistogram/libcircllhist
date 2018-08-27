from setuptools import setup, find_packages
setup(
    name="circllhist",
    version="0.2.1",
    description="Circonus log-linear histogram library",
    maintainer="Heinrich Hartmann",
    maintainer_email="heinrich.hartmann@circonus.com",
    url="https://github.com/circonus-labs/libcircllhist",
    install_requires=['cffi'],
    packages=['circllhist'],
)
