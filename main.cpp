#include <iostream>
#include <algorithm>
#include <string>
#include <vector>
#include <unordered_map>

struct Entry {
	std::string title;
	uint64_t created { 0 };
	uint64_t deleted { 0 };
	/**
	 * An auxiliary field that gets used for comparison of Entry instances.
	 * Avoiding branching at every comparison is its purpose.
	 * It must be assigned by JSON loading core based on {@code created} and {@code deleted} values.
	 */
	uint64_t timestamp { 0 };
	int num;

	bool operator<( const Entry &that ) const {
		return timestamp < that.timestamp;
	}
};

class MergeBuilder {
	std::unordered_map<int, const Entry *> buckets;
public:
	/**
	 * Tries to merge the supplied entries.
	 * @param entries a bunch of entries.
	 * @note the supplied entries are referred by their addresses.
	 * Entries are assumed to be valid and have a permanent address during the entire {@code MergeBuilder} object lifetime.
	 * These entries are assumed to be owned by something else.
	 */
	void addEntries( const std::vector<Entry> &entries );

	/**
	 * Creates a sorted by timestamp list of merged entries.
	 * @return a sorted list of pointers to entries that are assumed to be valid and owned by something else.
	 */
	std::vector<const Entry *> build();
};

void MergeBuilder::addEntries( const std::vector<Entry> &entries ) {
	for( const Entry &entry: entries ) {
		// Check whether there's an existing entry for the given `num`
		auto it = buckets.find( entry.num );
		// There's no such entry, perform an insertion
		if( it == buckets.end() ) {
			buckets.emplace( std::make_pair( entry.num, &entry ) );
			continue;
		}
		// Check whether the existing entry should be preserved
		const Entry &existing = *( it->second );
		if( !( existing < entry ) ) {
			continue;
		}
		// Overwrite the existing entry in-place.
		// Note that this is totally correct as the hash code is the same
		it->second = &entry;
	}
}

std::vector<const Entry *> MergeBuilder::build() {
	std::vector<const Entry *> result;
	for( const auto &[key, ref] : buckets ) {
		result.push_back( ref );
	}
	// Provide a proper comparator for sorting pointers to items
	auto cmp = []( const Entry *lhs, const Entry *rhs ) { return *lhs < *rhs; };
	std::sort( result.begin(), result.end(), cmp );
	return result;
}

static bool tryReadingEntries( const char *filename, std::vector<Entry> &output ) {
	return false;
}

static void printEntries( const std::vector<const Entry *> &entries ) {
}

int main( int argc, char **argv ) {
	if( argc < 3 ) {
		std::cerr << "Usage: mergelists-cpp <filename1> <filename2> ..." << std::endl;
		return 1;
	}

	// Content of all files is read first and is kept at a permanent address during the MergeBuilder lifetime.
	// The MergeBuilder operates on raw pointers to entries that are assumed to be owned by something else.
	std::vector<std::vector<Entry>> readLists;
	for( int i = 1; i < argc; ++i ) {
		std::vector<Entry> content;
		if( !::tryReadingEntries( argv[i], content ) ) {
			std::cerr << "Failed to read a file content of `" << argv[i] << " `" << std::endl;
			return 1;
		}
		readLists.emplace_back( std::move( content ) );
	}

	MergeBuilder builder;
	for( const auto &list: readLists ) {
		builder.addEntries( list );
	}

	printEntries( builder.build() );
	return 0;
}