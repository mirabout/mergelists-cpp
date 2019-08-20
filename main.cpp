#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>

#include <nlohmann/json.hpp>

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
	for( const auto &kvPair : buckets ) {
		result.push_back( kvPair.second );
	}
	// Provide a proper comparator for sorting pointers to items
	auto cmp = []( const Entry *lhs, const Entry *rhs ) { return *lhs < *rhs; };
	std::sort( result.begin(), result.end(), cmp );
	return result;
}

// Wrap keys once for faster access in the parsing loop
static std::string FIELD_NUM( "num" );
static std::string FIELD_TITLE( "title" );
static std::string FIELD_CREATED( "created" );
static std::string FIELD_DELETED( "deleted" );

template <typename T>
static bool getField( const nlohmann::json &json, const std::string &key, T &result, std::string &error ) {
	auto it = json.find( key );
	if( it != json.end() ) {
		result = *it;
		return true;
	}
	error = std::string( "Failed to get field `" ) + key + "` of an entry";
	return false;
}

template <typename T>
static bool tryGettingField( const nlohmann::json &json, const std::string &key, T &result ) {
	auto it = json.find( key );
	if( it != json.end() ) {
		result = *it;
		return true;
	}
	return false;
}

static bool tryParsingEntries( const nlohmann::json &root, std::vector<Entry> &output, std::string &error ) {
	if( !root.is_array() ) {
		error = "The root JSON object is not an array";
		return false;
	}

	std::vector<Entry> result;
	for( const nlohmann::json &elem: root ) {
		if( !elem.is_object() ) {
			error = "An element of a root JSON array is not an object";
			return false;
		}
		Entry entry;
		if( !::getField( elem, FIELD_NUM, entry.num, error ) ) {
			return false;
		}
		if( !::getField( elem, FIELD_TITLE, entry.title, error ) ) {
			return false;
		}
		assert( entry.created == 0 && entry.deleted == 0 );
		const bool hasCreated = ::tryGettingField( elem, FIELD_CREATED, entry.created );
		const bool hasDeleted = ::tryGettingField( elem, FIELD_DELETED, entry.deleted );
		if( hasCreated && hasDeleted ) {
			error = "Both `created` and `deleted` fields are present";
			return false;
		}
		if( !( hasCreated || hasDeleted ) ) {
			error = "Both `created` and `deleted` fields are absent";
			return false;
		}
		if( hasCreated ) {
			entry.timestamp = entry.created;
		}
		if( hasDeleted ) {
			entry.timestamp = entry.deleted;
		}
		result.emplace_back( std::move( entry ) );
	}

	// There's nothing that could throw left. Commit changes.
	output.clear();
	std::swap( result, output );
	return true;
}

static bool tryReadingEntries( const char *filename, std::vector<Entry> &output, std::string &error ) {
	try {
		std::ifstream stream;
		stream.open( filename, std::ios_base::in );
		if( !stream.is_open() ) {
			error = "Failed to open a file stream";
			return false;
		}
		nlohmann::json root( nlohmann::json::parse( stream ) );
		return tryParsingEntries( root, output, error );
	} catch( std::exception &ex ) {
		error = ex.what();
		return false;
	}
}

static void printEntries( const std::vector<const Entry *> &entries ) {
	nlohmann::json root;
	for( const Entry *entry : entries ) {
		nlohmann::json obj;
		obj[FIELD_NUM] = entry->num;
		obj[FIELD_TITLE] = entry->title;
		if( entry->created ) {
			obj[FIELD_CREATED] = entry->created;
		}
		if( entry->deleted ) {
			obj[FIELD_DELETED] = entry->deleted;
		}
		root.emplace_back( std::move( obj ) );
	}
	// Dump to the std::cout using pretty-printing
	std::cout << root.dump(2) << std::endl;
}

int main( int argc, char **argv ) {
	if( argc < 3 ) {
		std::cerr << "Usage: mergelists-cpp <filename1> <filename2> ..." << std::endl;
		return 1;
	}

	// Content of all files is read first and is kept at a permanent address during the MergeBuilder lifetime.
	// The MergeBuilder operates on raw pointers to entries that are assumed to be owned by something else.
	std::vector<std::vector<Entry>> readLists;

	std::string error;
	for( int i = 1; i < argc; ++i ) {
		std::vector<Entry> content;
		if( !::tryReadingEntries( argv[i], content, error ) ) {
			std::cerr << "Failed to read a file content of `" << argv[i] << " `: " << error << std::endl;
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