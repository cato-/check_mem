#include <sys/sysinfo.h>
#include <array>
#include <iostream>
#include <iomanip>
#include <tclap/CmdLine.h>

enum class status : int { OK = 0, WARNING = 1, CRITICAL = 2, UNKNOWN = 3 };
enum class valueType : char { VALUE, MAX, WARNING, CRITICAL };
enum class unitType : char { RAW, HUMAN, PERCENTAGE };
enum class unit : char { BYTE, KILOBYTE, MEGABYTE, GIGABYTE, TERABYTE, PETABYTE, EXABYTE };

const std::array<std::string, 7> units = { "B", "kB", "MB", "GB", "TB", "PB", "EB" };

std::string status_name( status s )
{
   switch( s )
   {
      case status::OK:
         return "OK";
         break;
      case status::WARNING:
         return "WARNING";
         break;
      case status::CRITICAL:
         return "CRITICAL";
         break;
      case status::UNKNOWN:
         return "UNKNOWN";
         break;
   }
   return "";
}

constexpr valueType getValueTypeForStatus( const status s )
{
  return ( s == status::WARNING ) ? valueType::WARNING : ( ( s == status::CRITICAL ) ? valueType::CRITICAL : valueType::VALUE );
}

class Value
{
   const unsigned long value;
   unsigned long max = -1;
   double warningPercentage = -1;
   double criticalPercentage = -1;
   
public:
   Value( unsigned long value ) : value( value ) {}

   void setMaximum( unsigned long m )
   {
      max = m;
   }

   void setLimits( double w, double c )
   {
      warningPercentage = w;
      criticalPercentage = c;
   }

   template< valueType T >
   double getRaw() const
   {
      switch ( T )
      {
         case valueType::VALUE:
            return value;
            break;
         case valueType::MAX:
            return max;
            break;
         case valueType::WARNING:
            return warningPercentage / 100. * max;
            break;
         case valueType::CRITICAL:
            return criticalPercentage / 100. * max;
            break;
      }
      throw;
   }

   template< valueType T, unitType U >
   double get( unsigned int bytesPerUnit = 1 ) const
   {
      const double v = getRaw< T >();
      switch ( U )
      {
         case unitType::RAW:
            return v;
            break;
         case unitType::PERCENTAGE:
            return static_cast< unsigned long >( 10000.0 * v / max ) / 100.;
            break;
         case unitType::HUMAN:
            return static_cast< unsigned long >( 1000.0 * v / bytesPerUnit ) / 1000.;
            break;
      }
      throw;
   }

   template< bool LESS >
   status check( double &exceededValue ) const
   {
      const double val = get< valueType::VALUE, unitType::PERCENTAGE >();
      const bool exceededCritical = LESS ? ( val < criticalPercentage ) : ( val > criticalPercentage );
      if ( criticalPercentage > 0 && exceededCritical )
      {
         exceededValue = criticalPercentage;
         return status::CRITICAL;
      }
      const bool exceededWarning = LESS ? ( val < warningPercentage ) : ( val > warningPercentage );
      if ( warningPercentage > 0 && exceededWarning )
      {
         exceededValue = warningPercentage;
         return status::WARNING;
      }
      return status::OK;
   }
   
   template< unitType U >
   std::string getPerfData( unsigned int bytesPerUnit, int unit ) const
   {
      std::stringstream ss;
      // Value and Unit
      ss << std::fixed << get< valueType::VALUE, U >( bytesPerUnit ) << ( U == unitType::PERCENTAGE ? "%" : units[ unit ] ) << ";";

      // Warning and Critical Values
      if ( warningPercentage > 0 && criticalPercentage > 0 )
         ss << std::fixed << get< valueType::WARNING, U >( bytesPerUnit ) << ";" << get< valueType::CRITICAL, U >( bytesPerUnit ) << ";";
      else
         ss << "U;U;";

      // Minimum Value
      ss << "0;";

      // Maximum Value
      ss << std::fixed << get< valueType::MAX, U>( bytesPerUnit );
      return ss.str();
   }
};

template< bool LESS >
void check( const Value &v, status &s, std::string &reason )
{
   double exceededValue = 0;
   const status cs = v.check< LESS >( exceededValue );
   if ( cs > s )
   {
      s = cs;
      std::stringstream ss;
      std::string comp = LESS ? " < " : " > ";
      ss << v.get< valueType::VALUE, unitType::PERCENTAGE >() << "%" << comp << exceededValue << "%";
      reason = ss.str();
   }
}

int main (int argc, char *argv[])
{
   TCLAP::ValueArg<unsigned int> unitArg("u","unit","unit for performance data (exponent of 1024, e.g. 0 for B, 1 for kB, default: 2)",false,2,"UNITEXPONENT");
   TCLAP::ValueArg<double> freeWarningArg("","free-warning","set warning threshold level for free memory (below %)",false,10,"PERCENTAGE");
   TCLAP::ValueArg<double> freeCriticalArg("","free-critical","set critical threshold level for free memory (below %)",false,5,"PERCENTAGE");
   TCLAP::ValueArg<double> usedWarningArg("","used-warning","set warning threshold level for used memory (above %)",false,90,"PERCENTAGE");
   TCLAP::ValueArg<double> usedCriticalArg("","used-critical","set critical threshold level for used memory (above %)",false,95,"PERCENTAGE");
   TCLAP::ValueArg<double> bufferWarningArg("","buffer-warning","set warning threshold level for buffer memory (above %)",false,90,"PERCENTAGE");
   TCLAP::ValueArg<double> bufferCriticalArg("","buffer-critical","set critical threshold level for buffer memory (above %)",false,95,"PERCENTAGE");
   TCLAP::ValueArg<double> sharedWarningArg("","shared-warning","set warning threshold level for shared memory (above %)",false,90,"PERCENTAGE");
   TCLAP::ValueArg<double> sharedCriticalArg("","shared-critical","set critical threshold level for shared memory (above %)",false,95,"PERCENTAGE");
   try {  
      TCLAP::CmdLine cmd("", ' ', "1.0");
      cmd.add( unitArg );
      cmd.add( freeWarningArg );
      cmd.add( freeCriticalArg );
      cmd.add( usedWarningArg );
      cmd.add( usedCriticalArg );
      cmd.add( bufferWarningArg );
      cmd.add( bufferCriticalArg );
      cmd.add( sharedWarningArg );
      cmd.add( sharedCriticalArg );
      cmd.parse( argc, argv );
   } 
   catch (TCLAP::ArgException &e)  // catch any exceptions
   {
      std::cout << "error: " << e.error() << " for arg " << e.argId() << std::endl;
      return static_cast< int >( status::UNKNOWN );
   }

   struct sysinfo si;
   int err = sysinfo( &si );
   if ( err )
   {
      std::cout << "UNKNOWN: Could not gather sysinfo() stats" << std::endl;
      return static_cast< int >( status::UNKNOWN );
   }

   const unsigned int unit = unitArg.getValue();
   const unsigned int bytesPerUnit = std::pow(1024, unit) / si.mem_unit;
   Value total( si.totalram );
   Value free( si.freeram );
   free.setMaximum( si.totalram );
   Value shared( si.sharedram );
   shared.setMaximum( si.totalram );
   Value buffer( si.bufferram );
   buffer.setMaximum( si.totalram );
   Value used( si.totalram - si.freeram - si.sharedram - si.bufferram );
   used.setMaximum( si.totalram );


   if ( freeWarningArg.isSet() && freeCriticalArg.isSet() )
     free.setLimits( freeWarningArg.getValue(), freeCriticalArg.getValue() );
   if ( usedWarningArg.isSet() && usedCriticalArg.isSet() )
     used.setLimits( usedWarningArg.getValue(), usedCriticalArg.getValue() );
   if ( bufferWarningArg.isSet() && bufferCriticalArg.isSet() )
     buffer.setLimits( bufferWarningArg.getValue(), bufferCriticalArg.getValue() );
   if ( sharedWarningArg.isSet() && sharedCriticalArg.isSet() )
     shared.setLimits( sharedWarningArg.getValue(), sharedCriticalArg.getValue() );

   status s = status::OK;
   std::string reason;
   check< true >( free, s, reason );
   check< false >( shared, s, reason );
   check< false >( buffer, s, reason );
   check< false >( used, s, reason );

   std::stringstream ss;
   ss << status_name(s) << ": " << reason;

   ss << "|'used'=" << used.getPerfData< unitType::HUMAN >( bytesPerUnit, unit );
   ss << "|'free'=" << free.getPerfData< unitType::HUMAN >( bytesPerUnit, unit );
   ss << "|'shared'=" << shared.getPerfData< unitType::HUMAN >( bytesPerUnit, unit );
   ss << "|'buffer'=" << buffer.getPerfData< unitType::HUMAN >( bytesPerUnit, unit );

   ss << "|'used'=" << used.getPerfData< unitType::PERCENTAGE >( bytesPerUnit, unit );
   ss << "|'free'=" << free.getPerfData< unitType::PERCENTAGE >( bytesPerUnit, unit );
   ss << "|'shared'=" << shared.getPerfData< unitType::PERCENTAGE >( bytesPerUnit, unit );
   ss << "|'buffer'=" << buffer.getPerfData< unitType::PERCENTAGE >( bytesPerUnit, unit );

   std::cout << ss.str() << std::endl;
   return static_cast< int >( s );
}
