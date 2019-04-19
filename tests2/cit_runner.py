#!/usr/bin/env python
import unittest
import argparse

BMC_START_DIR = '/usr/local/bin/tests2/tests/'

class RunTest:

    def __init__(self):
        self.testrunner = unittest.TextTestRunner(verbosity=2)
        self.testloader = unittest.defaultTestLoader

    def get_single_test(self, test_path):
        return self.testloader.loadTestsFromName(test_path)

    def run_single_test(self, test_path):
        """
        test_path example - tests.wedge100.test_eeprom.EepromTest.test_odm_pcb
        """
        return self.testrunner.run(self.get_single_test(test_path))

    def get_multiple_tests(self, test_paths):
        return self.testloader.loadTestsFromNames(test_paths)

    def run_multiple_tests(self, test_paths):
        self.testrunner.run(self.get_multiple_tests(test_paths))


class Tests:

    def __init__(self, platform, start_dir=BMC_START_DIR):
        self.platform = platform
        self.tests_set = []
        self.formatted_tests_set = []
        self.start_dir = start_dir + args.platform + '/'

    def discover_tests(self):
        suite = unittest.defaultTestLoader.discover(self.start_dir)
        return suite

    def get_tests(self, suite):
        if hasattr(suite, '__iter__'):
            for item in suite:
                self.get_tests(item)
        else:
            if 'common' not in str(suite):
                self.tests_set.append(str(suite))
        return self.tests_set

    def format_into_test_path(self, testitem):
        '''
        Given string like "test_odm_pcb (tests.wedge100.test_eeprom.EepromTest)"
        convert to a python test format to use it directly
        return value example : tests.wedge100.test_eeprom.EepromTest.test_odm_pcb
        '''
        test_string = testitem.split('(')
        test_path = test_string[1].split(')')[0] + '.' + test_string[0]
        return test_path.strip()

    def get_all_platform_tests(self):
        '''
        Returns a set of test names that are formatted to a single test like
        'tests.wedge100.test_eeprom.EepromTest.test_odm_pcb'
        '''
        for testitem in self.get_tests(self.discover_tests()):
            prefix = 'tests.' + self.platform + '.'
            self.formatted_tests_set.append(prefix+self.format_into_test_path(testitem))
        return self.formatted_tests_set


def arg_parser():
    parser = argparse.ArgumentParser()
    parser.add_argument('--list-tests',
                        action='store_true',
                        help="List all available tests")

    parser.add_argument('--platform',
                        help="Run all tests in platform by platform name",
                        choices=['wedge', 'wedge100', 'galaxy100', 'cmm', 'minipack', 'fbtp', 'fby2', 'yosemite', 'lightning', 'fbttn'])

    parser.add_argument('--run-test',
                        help="Path to run a single test. Example: \
                        tests.wedge100.test_eeprom.EepromTest.test_odm_pcb")

    parser.add_argument('--start-dir',
                        help="Path for where test discovery should start \
                        default: /usr/local/bin/tests2/tests/",
                        default=BMC_START_DIR)

    return parser.parse_args()

if __name__ == '__main__':
   args = arg_parser()

   if args.run_test:
       test_result = RunTest().run_single_test(args.run_test)
       rc = 0 if test_result.wasSuccessful() else 1
       exit(rc)
   elif args.platform:
       test_paths = Tests(args.platform, args.start_dir).get_all_platform_tests()
       if args.list_tests:
           for item in test_paths:
               print(item)
       else:
           RunTest().run_multiple_tests(test_paths)
   else:
        print("Platform name needed")
        exit(1)
