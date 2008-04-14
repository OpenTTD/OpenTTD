/* $Id$ */

/** @file timetable.h */

#ifndef TIMETABLE_H
#define TIMETABLE_H

void ShowTimetableWindow(const Vehicle *v);
void UpdateVehicleTimetable(Vehicle *v, bool travelling);
void SetTimetableParams(int param1, int param2, uint32 time);

#endif /* TIMETABLE_H */
