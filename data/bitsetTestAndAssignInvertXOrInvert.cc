#include "sys.h"
#include "debug.h"
#include "bitsetTest.h"
#include "bitsetTestTriop.h"

void bitsetTest::testAndAssignInvertXOrInvert(void)
{
  TESTTRIOPLEVEL2(&=, ^, ~, ~);
}
