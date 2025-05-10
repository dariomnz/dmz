import os
import lit.formats

config.name = "DMZ"
config.test_format = lit.formats.ShTest(False if os.name == 'nt' else True)

config.suffixes = ['.dmz']
