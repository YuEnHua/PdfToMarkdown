#include <gtest/gtest.h>

#include "pdf_to_md/error_codes.h"

TEST(ErrorCodes, OkIsZero) { EXPECT_EQ(PDFMD_OK, 0); }

TEST(ErrorCodes, CancelledCodeDistinct) {
    EXPECT_NE(PDFMD_ERR_CANCELLED, PDFMD_OK);
    EXPECT_NE(PDFMD_ERR_PDF_ENCRYPTED, PDFMD_ERR_PDF_OPEN);
    EXPECT_NE(PDFMD_ERR_MODEL_MISSING, PDFMD_ERR_OCR_INIT);
}

TEST(ErrorCodes, WriteAndBusyDistinct) {
    EXPECT_NE(PDFMD_ERR_WRITE_FAILED, PDFMD_ERR_BUSY);
}
