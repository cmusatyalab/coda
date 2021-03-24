from setuptools import setup
from codafs import __version__

setup(
    name="codafs",
    version=__version__,
    author="Jan Harkes",
    author_email="jaharkes@cs.cmu.edu",
    description="Python helpers for accessing Coda File System functionality",
    license="LGPLv2",
    url="https://github.com/cmusatyalab/coda",
    packages=["codafs"],
    install_requires=["attrs"],
    classifiers=[
        "Programming Language :: Python :: 3",
        "License :: OSI Approved :: GNU Lesser General Public License v2 (LGPLv2)",
    ],
)
