package com.tumuyan.ncnn.realsr;

import org.junit.Test;

import static org.junit.Assert.*;

/**
 * Regression tests for {@link SafPathHelper}.
 * <p>
 * These tests exercise the pure path-construction and display-name-extraction
 * logic that was previously embedded in DirectoryProcessActivity and broken
 * for non-primary storage volumes (SD cards, USB-OTG, third-party document
 * providers).
 * <p>
 * Methods under test:
 * <ul>
 *   <li>{@link SafPathHelper#resolveTreeDocIdToPath(String, String)}</li>
 *   <li>{@link SafPathHelper#extractTreeDisplayName(String)}</li>
 * </ul>
 */
public class SafPathHelperTest {

    private static final String STORAGE_BASE = "/storage/emulated/0";

    // ---------------------------------------------------------------
    // resolveTreeDocIdToPath — primary storage
    // ---------------------------------------------------------------

    @Test
    public void resolveTreeDocIdToPath_primary_root() {
        assertEquals(STORAGE_BASE + "/",
                SafPathHelper.resolveTreeDocIdToPath("primary:", STORAGE_BASE));
    }

    @Test
    public void resolveTreeDocIdToPath_primary_subdir() {
        assertEquals(STORAGE_BASE + "/DCIM",
                SafPathHelper.resolveTreeDocIdToPath("primary:DCIM", STORAGE_BASE));
    }

    @Test
    public void resolveTreeDocIdToPath_primary_nested() {
        assertEquals(STORAGE_BASE + "/DCIM/Camera",
                SafPathHelper.resolveTreeDocIdToPath("primary:DCIM/Camera", STORAGE_BASE));
    }

    @Test
    public void resolveTreeDocIdToPath_primary_deepNesting() {
        assertEquals(STORAGE_BASE + "/a/b/c/d/e",
                SafPathHelper.resolveTreeDocIdToPath("primary:a/b/c/d/e", STORAGE_BASE));
    }

    // ---------------------------------------------------------------
    // resolveTreeDocIdToPath — non-primary (SD card / USB-OTG)
    // ---------------------------------------------------------------

    @Test
    public void resolveTreeDocIdToPath_sdCard_subdir() {
        assertEquals("/storage/ABCD-1234/Photos",
                SafPathHelper.resolveTreeDocIdToPath("ABCD-1234:Photos", STORAGE_BASE));
    }

    @Test
    public void resolveTreeDocIdToPath_sdCard_root() {
        assertEquals("/storage/ABCD-1234/",
                SafPathHelper.resolveTreeDocIdToPath("ABCD-1234:", STORAGE_BASE));
    }

    @Test
    public void resolveTreeDocIdToPath_usbOtg() {
        assertEquals("/storage/1234-5678/DCIM",
                SafPathHelper.resolveTreeDocIdToPath("1234-5678:DCIM", STORAGE_BASE));
    }

    @Test
    public void resolveTreeDocIdToPath_nonPrimary_doesNotUseStorageBase() {
        // Even with a different storage base, non-primary always goes to /storage/<vol>/
        assertEquals("/storage/XYZ/path",
                SafPathHelper.resolveTreeDocIdToPath("XYZ:path", "/some/other/base"));
    }

    // ---------------------------------------------------------------
    // resolveTreeDocIdToPath — edge cases
    // ---------------------------------------------------------------

    @Test
    public void resolveTreeDocIdToPath_nullDocId_returnsEmpty() {
        assertEquals("", SafPathHelper.resolveTreeDocIdToPath(null, STORAGE_BASE));
    }

    @Test
    public void resolveTreeDocIdToPath_noColon_fallsBack() {
        // Third-party providers that return a docId without colon separator
        assertEquals(STORAGE_BASE + "/some-doc-id",
                SafPathHelper.resolveTreeDocIdToPath("some-doc-id", STORAGE_BASE));
    }

    @Test
    public void resolveTreeDocIdToPath_emptyString_fallsBack() {
        assertEquals(STORAGE_BASE + "/",
                SafPathHelper.resolveTreeDocIdToPath("", STORAGE_BASE));
    }

    @Test
    public void resolveTreeDocIdToPath_colonOnly() {
        // Edge: docId is just ":" → splits to ["", ""], non-primary branch
        assertEquals("/storage//",
                SafPathHelper.resolveTreeDocIdToPath(":", STORAGE_BASE));
    }

    @Test
    public void resolveTreeDocIdToPath_primaryCaseSensitive() {
        // "Primary" (capital P) should NOT match — treated as non-primary volume
        assertEquals("/storage/Primary/DCIM",
                SafPathHelper.resolveTreeDocIdToPath("Primary:DCIM", STORAGE_BASE));
    }

    // ---------------------------------------------------------------
    // extractTreeDisplayName
    // ---------------------------------------------------------------

    @Test
    public void extractTreeDisplayName_primary_singleDir() {
        assertEquals("DCIM", SafPathHelper.extractTreeDisplayName("primary:DCIM"));
    }

    @Test
    public void extractTreeDisplayName_primary_nestedPath() {
        assertEquals("Camera", SafPathHelper.extractTreeDisplayName("primary:DCIM/Camera"));
    }

    @Test
    public void extractTreeDisplayName_primary_deepNested() {
        assertEquals("e", SafPathHelper.extractTreeDisplayName("primary:a/b/c/d/e"));
    }

    @Test
    public void extractTreeDisplayName_sdCard() {
        assertEquals("Photos", SafPathHelper.extractTreeDisplayName("ABCD-1234:Photos"));
    }

    @Test
    public void extractTreeDisplayName_sdCard_nested() {
        assertEquals("Vacation", SafPathHelper.extractTreeDisplayName("ABCD-1234:Photos/Vacation"));
    }

    @Test
    public void extractTreeDisplayName_noColon() {
        assertEquals("some-doc", SafPathHelper.extractTreeDisplayName("some-doc"));
    }

    @Test
    public void extractTreeDisplayName_nullDocId_returnsNull() {
        assertNull(SafPathHelper.extractTreeDisplayName(null));
    }

    @Test
    public void extractTreeDisplayName_emptyString_returnsNull() {
        assertNull(SafPathHelper.extractTreeDisplayName(""));
    }

    @Test
    public void extractTreeDisplayName_primary_emptyPath() {
        // "primary:" → split[1] is empty → falls through to return raw docId
        assertEquals("primary:", SafPathHelper.extractTreeDisplayName("primary:"));
    }

    @Test
    public void extractTreeDisplayName_primary_trailingSlash() {
        // "primary:DCIM/" → path ends with slash → returns full path segment
        assertEquals("DCIM/", SafPathHelper.extractTreeDisplayName("primary:DCIM/"));
    }

    // ---------------------------------------------------------------
    // Regression scenario: SD card that used to fail with File.exists()
    // ---------------------------------------------------------------

    @Test
    public void regression_sdCard_pathNotUnderPrimaryStorage() {
        // Before the fix, an SD card docId like "ABCD-1234:DCIM" would be
        // converted to "/storage/ABCD-1234/DCIM", and then
        // new File("/storage/ABCD-1234/DCIM").exists() would return false
        // on many devices, blocking the Start button.
        //
        // Now the path is still derived (for the native binary), but
        // validation uses the SAF URI, not File.exists().
        //
        // This test verifies the path is correctly derived regardless.
        String path = SafPathHelper.resolveTreeDocIdToPath("ABCD-1234:DCIM", STORAGE_BASE);
        assertEquals("/storage/ABCD-1234/DCIM", path);

        // And the display name is human-readable
        assertEquals("DCIM", SafPathHelper.extractTreeDisplayName("ABCD-1234:DCIM"));
    }

    @Test
    public void regression_vendorProvider_nonStandardDocId() {
        // Some vendor document providers return docIds that don't follow
        // the "type:path" convention. The old code returned "" for these,
        // causing the EditText to show the raw URI string.
        String path = SafPathHelper.resolveTreeDocIdToPath("my-doc-folder", STORAGE_BASE);
        assertEquals(STORAGE_BASE + "/my-doc-folder", path);
    }
}
