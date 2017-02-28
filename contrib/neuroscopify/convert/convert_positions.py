
import csv

def convert_positions(src_fname, dest_fname, frequency):
    channel_data = dict()
    with open(src_fname) as csvfile:
        f = open(dest_fname, 'w')
        csvreader = csv.reader(csvfile, delimiter=';', quotechar='|')
        first = True

        expected_step_size = 1000 / frequency

        last_time = 0;
        last_skipped = False
        for row in csvreader:
            if first:
                first = False
                continue

            row[0] = int(row[0])

            step_size = row[0] - last_time
            last_time = row[0]

            skip = False
            if ((step_size + 8) > expected_step_size):
                skip = True

            if skip:
                last_skipped = True
                print(step_size)
                # add dummy entry instead
                f.write('{0} {1}\n'.format(-1, -1))
                continue

            last_skipped = False

            f.write('{0} {1}\n'.format(row[1], row[2]))
