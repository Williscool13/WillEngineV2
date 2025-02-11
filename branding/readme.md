# Branding

The create_regs.bat file creates (does not run!) .reg files to associate .willmodel and .willmap files with custom logos. This is not at all essential to run the program, but it does make the logo of my engine-specific files look pretty.


## willmodel_association.reg:

HKEY_CLASSES_ROOT.willmodel
HKEY_CLASSES_ROOT\WillEngineModel
HKEY_CLASSES_ROOT\WillEngineModel\DefaultIcon

## willmap_association.reg:

HKEY_CLASSES_ROOT.willmap
HKEY_CLASSES_ROOT\WillEngineMap
HKEY_CLASSES_ROOT\WillEngineMap\DefaultIcon