# Release procedure

* Edit setup.py and increase version number
* Commit the change with a message: `Version bump to <version>`
* Do a clean build of the C library to ensure the FFI wrapper is up to date
* Upload to PyPI:
  * To test: `twine upload --repository testpypi dist/*`
  * For real: `twine upload dist/*`
