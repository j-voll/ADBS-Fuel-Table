import csv
import os
import sys
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.ticker import AutoMinorLocator

def plot_combined_data(input_file):
    """
    Create a combined plot with multiple y-axes:
    - Primary Y-axis: Pitch (degrees)
    - Secondary Y-axes: Fuel Level and Temperatures
    - X-axis: Time (seconds)
    
    This shows how pitch changes affect fuel level and temperatures.
    """
    # Initialize lists to store data
    times = []
    pitches = []
    fuel_levels = []
    internal_temps = []
    external_temps = []
    phases = []
    
    # Phase markers for the plot
    phase_markers = {
        'Movement': {'marker': 'o', 'color': 'blue'},
        'Stationary1': {'marker': 's', 'color': 'green'},
        'Stationary2': {'marker': 's', 'color': 'darkgreen'},
        'ReturnToZero': {'marker': '^', 'color': 'orange'},
        'Complete': {'marker': 'x', 'color': 'red'}
    }
    
    try:
        with open(input_file, 'r', newline='') as infile:
            reader = csv.reader(infile)
            
            # Process header row
            header = next(reader)
            
            # Find the column indices
            time_index = header.index('TimeMS') if 'TimeMS' in header else 0
            fuel_level_index = header.index('FuelLevel') if 'FuelLevel' in header else 1
            internal_temp_index = header.index('InternalTemp') if 'InternalTemp' in header else 2
            external_temp_index = header.index('ExternalTemp') if 'ExternalTemp' in header else 3
            pitch_index = header.index('Pitch') if 'Pitch' in header else 4
            phase_index = header.index('Phase') if 'Phase' in header else 5
            
            # Process data rows
            for row in reader:
                if len(row) > max(time_index, pitch_index, phase_index, fuel_level_index, internal_temp_index, external_temp_index):
                    # Convert time from milliseconds to seconds
                    try:
                        time_ms = float(row[time_index])
                        time_sec = time_ms / 1000.0
                        times.append(time_sec)
                    except ValueError:
                        continue
                    
                    # Get pitch value
                    try:
                        pitch = float(row[pitch_index])
                        pitches.append(pitch)
                    except ValueError:
                        pitches.append(np.nan)
                    
                    # Get fuel level
                    try:
                        if row[fuel_level_index] != "No Data":
                            fuel_level = float(row[fuel_level_index])
                            fuel_levels.append(fuel_level)
                        else:
                            fuel_levels.append(np.nan)
                    except ValueError:
                        fuel_levels.append(np.nan)
                    
                    # Get internal temperature
                    try:
                        if row[internal_temp_index] != "No Data":
                            internal_temp = float(row[internal_temp_index])
                            internal_temps.append(internal_temp)
                        else:
                            internal_temps.append(np.nan)
                    except ValueError:
                        internal_temps.append(np.nan)
                    
                    # Get external temperature
                    try:
                        if row[external_temp_index] != "No Data" and row[external_temp_index] != "Disabled" and \
                           row[external_temp_index] != "Open Circuit" and row[external_temp_index] != "Short Circuit":
                            external_temp = float(row[external_temp_index])
                            external_temps.append(external_temp)
                        else:
                            external_temps.append(np.nan)
                    except ValueError:
                        external_temps.append(np.nan)
                    
                    # Get phase
                    phase = row[phase_index] if phase_index < len(row) else "Unknown"
                    phases.append(phase)
        
        # Create figure and primary axis for pitch
        fig, ax1 = plt.subplots(figsize=(14, 8))
        
        # Plot pitch on primary y-axis
        color = 'tab:blue'
        ax1.set_xlabel('Time (seconds)', fontsize=12)
        ax1.set_ylabel('Pitch (degrees)', color=color, fontsize=12)
        ax1.plot(times, pitches, color=color, linewidth=2, label='Pitch')
        ax1.tick_params(axis='y', labelcolor=color)
        ax1.grid(True, linestyle='--', alpha=0.7)
        
        # Add phase markers
        unique_phases = []
        for i in range(len(times)):
            if i % 100 == 0:  # Add marker every 100 points to avoid overcrowding
                phase = phases[i]
                if phase not in unique_phases:
                    unique_phases.append(phase)
                    marker_config = phase_markers.get(phase, {'marker': 'o', 'color': 'black'})
                    ax1.scatter(times[i], pitches[i], 
                               color=marker_config['color'], 
                               marker=marker_config['marker'], 
                               s=100, label=phase)
        
        # Create secondary y-axis for fuel level
        ax2 = ax1.twinx()
        color = 'tab:red'
        ax2.set_ylabel('Fuel Level', color=color, fontsize=12)
        ax2.plot(times, fuel_levels, color=color, linestyle='--', linewidth=2, label='Fuel Level')
        ax2.tick_params(axis='y', labelcolor=color)
        
        # Create third y-axis for temperatures
        ax3 = ax1.twinx()
        # Offset the third axis
        ax3.spines["right"].set_position(("axes", 1.1))
        color = 'tab:green'
        ax3.set_ylabel('Temperature', color=color, fontsize=12)
        ax3.plot(times, internal_temps, color=color, linestyle='-.', linewidth=2, label='Internal Temp')
        ax3.plot(times, external_temps, color='tab:purple', linestyle=':', linewidth=2, label='External Temp')
        ax3.tick_params(axis='y', labelcolor=color)
        
        # Add minor grid lines
        ax1.xaxis.set_minor_locator(AutoMinorLocator())
        ax1.yaxis.set_minor_locator(AutoMinorLocator())
        ax1.grid(which='minor', linestyle=':', alpha=0.4)
        
        # Add title
        plt.title('Pitch, Fuel Level, and Temperature vs Time', fontsize=16, fontweight='bold')
        
        # Create combined legend
        lines1, labels1 = ax1.get_legend_handles_labels()
        lines2, labels2 = ax2.get_legend_handles_labels()
        lines3, labels3 = ax3.get_legend_handles_labels()
        
        ax1.legend(lines1 + lines2 + lines3, labels1 + labels2 + labels3, 
                   loc='upper center', bbox_to_anchor=(0.5, -0.12), ncol=4, fontsize=10,
                   frameon=True, facecolor='white', edgecolor='black')
        
        # Create output filename based on input filename
        base_name = os.path.splitext(input_file)[0]
        output_file = f"{base_name}_combined_plot.png"
        pdf_output = f"{base_name}_combined_plot.pdf"
        
        # Save figure
        plt.tight_layout()
        plt.subplots_adjust(bottom=0.2)  # Make room for the legend
        plt.savefig(output_file, dpi=300, bbox_inches='tight')
        plt.savefig(pdf_output, bbox_inches='tight')
        
        plt.show()
        
        print(f"Plots saved to:")
        print(f"  PNG: {output_file}")
        print(f"  PDF: {pdf_output}")
        return output_file
    
    except Exception as e:
        print(f"Error plotting data: {e}")
        import traceback
        traceback.print_exc()
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
    
    # Plot the data
    plot_combined_data(input_file)

if __name__ == "__main__":
    main()