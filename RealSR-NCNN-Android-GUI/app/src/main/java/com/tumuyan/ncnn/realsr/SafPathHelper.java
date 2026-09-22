package com.tumuyan.ncnn.realsr;

import android.content.Context;
import android.net.Uri;
import android.os.Environment;

import androidx.documentfile.provider.DocumentFile;

/**
 * Helper utilities for Storage Access Framework (SAF) tree URIs.
 * <p>
 * Provides methods to:
 * <ul>
 *   <li>Check whether a URI is a SAF content URI</li>
 *   <li>Validate a SAF tree URI via DocumentFile</li>
 *   <li>Derive a best-effort filesystem path from a tree URI</li>
 *   <li>Extract a display name from a tree URI</li>
 * </ul>
 */
public class SafPathHelper {

    /**
     * Checks whether the given URI is a SAF content URI (scheme == "content").
     * Returns false for null or non-content URIs.
     */
    public static boolean isSafUri(Uri uri) {
        return uri != null && "content".equals(uri.getScheme());
    }

    /**
     * Validates a SAF tree URI by wrapping it as a DocumentFile and checking
     * that it represents an existing, accessible directory.
     *
     * @param uri     a tree URI previously obtained from ACTION_OPEN_DOCUMENT_TREE
     * @param context any context used to resolve the content provider
     * @return true if the URI is a valid, accessible directory; false otherwise
     */
    public static boolean isValidTreeUri(Uri uri, Context context) {
        if (uri == null || context == null) return false;
        try {
            DocumentFile doc = DocumentFile.fromTreeUri(context, uri);
            return doc != null && doc.exists() && doc.isDirectory();
        } catch (Exception e) {
            return false;
        }
    }

    /**
     * Best-effort conversion of a tree-URI document ID to a filesystem path.
     * <p>
     * Handles two common patterns:
     * <ol>
     *   <li>{@code primary:<relative>} → {@code <external-storage>/<relative>}</li>
     *   <li>{@code <volume-id>:<relative>} → {@code /storage/<volume-id>/<relative>}</li>
     * </ol>
     * Falls back to {@link Environment#getExternalStorageDirectory()} when the
     * document ID does not contain a colon separator.
     *
     * @param docId the tree document ID (e.g. "primary:DCIM", "ABCD-1234:Photos")
     * @return an absolute filesystem path, or empty string if docId is null
     */
    public static String getAbsolutePathFromDocId(String docId) {
        return resolveTreeDocIdToPath(docId, Environment.getExternalStorageDirectory().toString());
    }

    /**
     * Pure path-construction logic shared by {@link #getAbsolutePathFromDocId(String)}
     * and unit tests.  Takes an explicit storage base so it can run on the JVM
     * without the Android framework.
     *
     * @param docId          the tree document ID (may be null)
     * @param storageBase    the external-storage root (e.g. "/storage/emulated/0")
     * @return an absolute path, or empty string when docId is null
     */
    static String resolveTreeDocIdToPath(String docId, String storageBase) {
        if (docId == null) return "";
        if (docId.contains(":")) {
            String[] split = docId.split(":", 2);
            if (split.length == 2) {
                if ("primary".equals(split[0])) {
                    return storageBase + "/" + split[1];
                } else {
                    return "/storage/" + split[0] + "/" + split[1];
                }
            }
        }
        // docId without colon separator (e.g. some third-party providers)
        return storageBase + "/" + docId;
    }

    /**
     * Extracts a human-readable display name from a tree URI.
     * <p>
     * For a document ID like "primary:DCIM/Photos", returns "Photos".
     * For "ABCD-1234:DCIM", returns "DCIM".
     * Falls back to the last path segment of the URI, or "output" if nothing usable.
     *
     * @param treeUri the tree URI
     * @return a display name suitable for showing to the user
     */
    public static String getDisplayNameFromTreeUri(Uri treeUri) {
        if (treeUri == null) return "output";
        try {
            String docId = getTreeDocumentIdSafe(treeUri);
            String result = extractTreeDisplayName(docId);
            if (result != null && !result.isEmpty()) {
                return result;
            }
        } catch (Exception ignored) {
        }
        String segment = treeUri.getLastPathSegment();
        return (segment != null && !segment.isEmpty()) ? segment : "output";
    }

    /**
     * Extracts a human-readable directory name from a tree document ID.
     * Pure string logic — no Android framework dependency.
     *
     * @param docId the tree document ID (e.g. "primary:DCIM/Photos")
     * @return the last path component, or null if docId is unusable
     */
    static String extractTreeDisplayName(String docId) {
        if (docId == null || docId.isEmpty()) return null;
        if (docId.contains(":")) {
            String[] split = docId.split(":", 2);
            if (split.length == 2 && !split[1].isEmpty()) {
                String path = split[1];
                int lastSlash = path.lastIndexOf('/');
                if (lastSlash >= 0 && lastSlash < path.length() - 1) {
                    return path.substring(lastSlash + 1);
                }
                return path;
            }
        }
        return docId;
    }

    /**
     * Safe wrapper around {@link android.provider.DocumentsContract#getTreeDocumentId(Uri)}
     * that catches IllegalArgumentException for malformed URIs.
     */
    private static String getTreeDocumentIdSafe(Uri treeUri) {
        try {
            return android.provider.DocumentsContract.getTreeDocumentId(treeUri);
        } catch (IllegalArgumentException e) {
            return null;
        }
    }

    /**
     * Convenience overload: extracts the tree document ID from a tree URI and
     * converts it to a best-effort filesystem path.
     *
     * @param treeUri a tree URI from ACTION_OPEN_DOCUMENT_TREE
     * @return an absolute filesystem path, or empty string on failure
     */
    public static String getAbsolutePathFromTreeUri(Uri treeUri) {
        String docId = getTreeDocumentIdSafe(treeUri);
        return getAbsolutePathFromDocId(docId);
    }
}
