import csv
import os
import sys
from datetime import datetime

def process_csv_file(input_file):
    """
    Process the CSV file to reformat fuel level and temperature data.
    Converts:
        - Fuel level
        - Internal temp
        - External temp
    """
    # Create output filename based on input filename
    base_name = os.path.splitext(input_file)[0]
    output_file = f"{base_name}_processed_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv"
    
    # Count of rows processed
    rows_processed = 0
    
    try:
        with open(input_file, 'r', newline='') as infile, open(output_file, 'w', newline='') as outfile:
            reader = csv.reader(infile)
            writer = csv.writer(outfile)
            
            # Process header row
            header = next(reader)
            writer.writerow(header)
            
            # Process data rows
            for row in reader:
                if len(row) >= 5:  # Make sure row has enough columns
                    # Process fuel level
                    try:
                        if row[1] != "No Data" and row[1].isdigit():
                            fuel_level = int(row[1])
                            row[1] = f"{fuel_level / 100:.2f}"
                    except (ValueError, IndexError):
                        pass
                    
                    # Process internal temperature
                    try:
                        if row[2] != "No Data" and row[2].isdigit():
                            internal_temp = int(row[2])
                            row[2] = f"{internal_temp / 100:.2f}"
                    except (ValueError, IndexError):
                        pass
                    
                    # Process external temperature
                    try:
                        if row[3] != "No Data" and row[3] != "Disabled" and row[3] != "Open Circuit" and row[3] != "Short Circuit" and row[3].isdigit():
                            external_temp = int(row[3])
                            row[3] = f"{external_temp / 100:.2f}"
                    except (ValueError, IndexError):
                        pass
                
                writer.writerow(row)
                rows_processed += 1
        
        print(f"Processing complete!")
        print(f"Processed {rows_processed} rows")
        print(f"Output saved to: {output_file}")
        return output_file
    
    except Exception as e:
        print(f"Error processing file: {e}")
        return None

def main():
    # Check if file was provided as command line argument
    if len(sys.argv) > 1:
        input_file = sys.argv[1]
    else:
        # Ask user for input file
        input_file = input("Enter path to CSV file: ").strip('"')
    
    # Check if file exists
    if not os.path.isfile(input_file):
        print(f"Error: File '{input_file}' not found.")
        return
    
    # Process the file
    process_csv_file(input_file)

if __name__ == "__main__":
    main()