package org.libelektra.keyExt

import org.libelektra.Key
import java.util.*
import org.libelektra.ReadableKey
import kotlin.math.log10

fun Key.isEmpty() = string.isEmpty()

fun Key.isNotEmpty() = !isEmpty()

fun Key.forEachKeyName(action: (String) -> Unit) {
    keyNameIterator().forEach(action)
}

fun <T> keyOf(name: String, value: T?): Key {
    return Key.create(name, value)
}

fun <T> keyOf(name: String, value: T?, vararg metaKeys: Key): Key {
    return Key.create(name, value).apply {
        metaKeys.forEach {
            setMeta(it.name, it.string)
        }
    }
}

fun <T> Optional<T>.orNull(): T? = orElse(null)

fun Key.isArray(): Boolean {
    val arrayKeyOptional = getMeta("array")
    if (arrayKeyOptional.isEmpty) {
        return false
    }

    return try {
        arrayKeyOptional.get().parseIndex()
        true
    } catch (e: NumberFormatException) {
        false
    }
}

fun Key.getLastArrayIndex(): Int {
    return getMeta("array").get().parseIndex()
}

fun ReadableKey.parseIndex() = this.string
        .removePrefix("#")
        .substringAfterLast("_")
        .toInt()

fun Int.toElektraArrayIndex(): String {
    val underscores = if (this > 0) {
        "_".repeat(
                log10(toDouble()).toInt()
        )
    } else {
        ""
    }
    return "#${underscores}${this}"
}
