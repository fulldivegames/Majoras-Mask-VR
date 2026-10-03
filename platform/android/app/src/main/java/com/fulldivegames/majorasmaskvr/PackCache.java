package com.fulldivegames.majorasmaskvr;

import java.io.*;
import java.nio.file.*;
import java.security.MessageDigest;
import java.util.UUID;

/** Filesystem-only pack import. Cache files are immutable and named by content. */
final class PackCache {
    static final long RESERVE_BYTES = 256L * 1024 * 1024;
    static final class Result {
        final String file, sha256;
        final long size;
        Result(String file, String sha256, long size) {
            this.file = file; this.sha256 = sha256; this.size = size;
        }
    }
    static boolean validName(String file) {
        return file != null && file.matches("[0-9a-f]{64}\\.(o2r|otr)");
    }
    static String hex(byte[] digest) {
        StringBuilder result = new StringBuilder(64);
        final char[] digits = "0123456789abcdef".toCharArray();
        for (byte value : digest) {
            result.append(digits[(value & 255) >>> 4]); result.append(digits[value & 15]);
        }
        return result.toString();
    }
    static Result hash(InputStream source, long expectedSize) throws Exception {
        if (source == null) throw new IOException("Cannot open pack");
        MessageDigest digest = MessageDigest.getInstance("SHA-256");
        long total = 0;
        try (InputStream in = source) {
            byte[] buffer = new byte[1024 * 1024]; int count;
            while ((count = in.read(buffer)) != -1) {
                total += count;
                if (expectedSize >= 0 && total > expectedSize) throw new IOException("Pack changed while being read");
                digest.update(buffer, 0, count);
            }
        }
        if (expectedSize >= 0 && total != expectedSize) throw new IOException("Incomplete pack");
        return new Result("", hex(digest.digest()), total);
    }
    static Result copy(InputStream source, File cache, String extension, long expectedSize) throws Exception {
        if (source == null) throw new IOException("Cannot open pack");
        File stage = new File(cache, UUID.randomUUID() + ".pending");
        // A large compatible pack is limited by actual available storage, not an
        // 8 GiB cap. Keep space for saves/settings and reject before truncating.
        final long available = Math.max(0, cache.getUsableSpace() - RESERVE_BYTES);
        long total = 0;
        MessageDigest digest = MessageDigest.getInstance("SHA-256");
        try (InputStream in = source) {
            if (!(".o2r".equals(extension) || ".otr".equals(extension))) throw new IOException("Unsupported pack extension");
            if (expectedSize > available) throw new IOException("Not enough free storage to import pack (including a 256 MiB reserve)");
            try (FileOutputStream out = new FileOutputStream(stage)) {
                byte[] buffer = new byte[1024 * 1024]; int count;
                while ((count = in.read(buffer)) != -1) {
                    total += count;
                    if (total > available) throw new IOException("Not enough free storage for pack");
                    if (expectedSize >= 0 && total > expectedSize) throw new IOException("Pack changed while being copied");
                    out.write(buffer, 0, count); digest.update(buffer, 0, count);
                }
                out.getFD().sync();
            }
            if (expectedSize >= 0 && total != expectedSize) throw new IOException("Incomplete pack");
            final String sha = hex(digest.digest());
            File target = new File(cache, sha + extension);
            if (target.isFile() && target.length() == total && hash(new FileInputStream(target), total).sha256.equals(sha)) {
                Files.delete(stage.toPath());
            } else {
                Files.move(stage.toPath(), target.toPath(), StandardCopyOption.REPLACE_EXISTING, StandardCopyOption.ATOMIC_MOVE);
            }
            return new Result(target.getName(), sha, total);
        } finally {
            // Failed imports never leave a multi-gigabyte staging file behind.
            if (stage.exists()) stage.delete();
        }
    }
}
