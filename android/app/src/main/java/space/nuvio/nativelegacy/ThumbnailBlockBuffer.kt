package space.nuvio.nativelegacy

/** One source-owned scratch buffer. The callback must consume/copy before return.
 * Holding this monitor through conversion prevents CPU/GPU consumers seeing an
 * overwritten block. Oversized reads preserve the old temporary-allocation path.
 */
internal class ThumbnailBlockBuffer(private val maxRetained: Int=8 shl 20) {
    private var buffer=ByteArray(0)
    private var allocations=0L
    private var reuses=0L
    private var oversizedAllocations=0L
    init {require(maxRetained>0)}
    @Synchronized fun <T> use(length: Int, consume: (ByteArray,Int)->T): T {
        require(length>0)
        if(length>maxRetained) {oversizedAllocations++;return consume(ByteArray(length),length)}
        if(buffer.size<length) {
            var capacity=65536.coerceAtMost(maxRetained)
            while(capacity<length) capacity=(capacity.toLong()*2).coerceAtMost(maxRetained.toLong()).toInt()
            buffer=ByteArray(capacity);allocations++
        } else reuses++
        return consume(buffer,length)
    }
    @Synchronized fun clear() {buffer=ByteArray(0)}
    @Synchronized fun summary()="allocations=$allocations reuses=$reuses oversizedAllocations=$oversizedAllocations retainedBytes=${buffer.size} capBytes=$maxRetained"
}
