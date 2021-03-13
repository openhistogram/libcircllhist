from setuptools import setup, find_packages

with open('README.md') as file:
    long_description = file.read()

setup(
    name="circllhist",
    long_description=long_description,
    version="0.3.0",
    description="OpenHistogram log-linear histogram library",
    maintainer="Heinrich Hartmann",
    maintainer_email="heinrich.hartmann@circonus.com",
    url="https://github.com/openhistogram/libcircllhist",
    install_requires=['cffi'],
    packages=['circllhist'],
)
